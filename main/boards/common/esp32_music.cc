#include "esp32_music.h"

#include "board.h"
#include "system_info.h"
#include "audio_codec.h"
#include "application.h"
#include "display.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <cJSON.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp_ae_rate_cvt.h"

#define TAG "Esp32Music"

#define MUSIC_RATE_CVT_CFG(_src_rate, _dest_rate)           \
    (esp_ae_rate_cvt_cfg_t) {                               \
        .src_rate = (uint32_t)(_src_rate),                  \
        .dest_rate = (uint32_t)(_dest_rate),                \
        .channel = (uint8_t)ESP_AUDIO_MONO,                 \
        .bits_per_sample = ESP_AUDIO_BIT16,                 \
        .complexity = 2,                                    \
        .perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED,       \
    }

// MP3 streams are typically 44100 Hz stereo; the board I2S TX is often 24000 Hz.
// Writing unresampled PCM plays slow and low. Recreate the converter if the
// source rate changes between songs.
static bool ResampleMusicPcm(esp_ae_rate_cvt_handle_t* rate_cvt, int* opened_src_rate, int src_rate,
                             int dest_rate, const int16_t* input, int input_samples,
                             std::vector<int16_t>& output) {
    if (input_samples <= 0) {
        output.clear();
        return true;
    }
    if (src_rate <= 0 || dest_rate <= 0) {
        return false;
    }
    if (src_rate == dest_rate) {
        output.assign(input, input + input_samples);
        return true;
    }
    if (*rate_cvt == nullptr || *opened_src_rate != src_rate) {
        if (*rate_cvt != nullptr) {
            esp_ae_rate_cvt_close(*rate_cvt);
            *rate_cvt = nullptr;
        }
        esp_ae_rate_cvt_cfg_t cfg = MUSIC_RATE_CVT_CFG(src_rate, dest_rate);
        auto ret = esp_ae_rate_cvt_open(&cfg, rate_cvt);
        if (*rate_cvt == nullptr) {
            ESP_LOGE(TAG, "Failed to open music resampler %d -> %d Hz, err=%d", src_rate, dest_rate,
                     ret);
            return false;
        }
        *opened_src_rate = src_rate;
        ESP_LOGI(TAG, "Music resampler %d -> %d Hz", src_rate, dest_rate);
    }

    uint32_t max_out = 0;
    if (esp_ae_rate_cvt_get_max_out_sample_num(*rate_cvt, (uint32_t)input_samples, &max_out) !=
        ESP_AE_ERR_OK) {
        return false;
    }
    output.resize(max_out);
    uint32_t actual = max_out;
    auto ret = esp_ae_rate_cvt_process(*rate_cvt, (esp_ae_sample_t)input, (uint32_t)input_samples,
                                       (esp_ae_sample_t)output.data(), &actual);
    if (ret != ESP_AE_ERR_OK) {
        ESP_LOGW(TAG, "Music resample failed: %d", ret);
        output.clear();
        return false;
    }
    output.resize(actual);
    return true;
}

static std::string url_encode(const std::string& str) {
    std::string encoded;
    char hex[4];

    for (size_t i = 0; i < str.length(); i++) {
        unsigned char c = str[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else if (c == ' ') {
            encoded += '+';
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", c);
            encoded += hex;
        }
    }
    return encoded;
}

Esp32Music::Esp32Music()
    : last_downloaded_data_(),
      current_music_url_(),
      current_song_name_(),
      is_playing_(false),
      is_downloading_(false),
      play_thread_(),
      download_thread_(),
      audio_buffer_(),
      buffer_mutex_(),
      buffer_cv_(),
      buffer_size_(0),
      mp3_decoder_(nullptr),
      mp3_frame_info_(),
      mp3_decoder_initialized_(false) {
    ESP_LOGI(TAG, "Music player initialized");
    InitializeMp3Decoder();
}

Esp32Music::~Esp32Music() {
    ESP_LOGI(TAG, "Destroying music player");
    is_downloading_ = false;
    is_playing_ = false;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    if (download_thread_.joinable()) {
        download_thread_.join();
    }
    if (play_thread_.joinable()) {
        play_thread_.join();
    }

    ClearAudioBuffer();
    CleanupMp3Decoder();
    ESP_LOGI(TAG, "Music player destroyed");
}

bool Esp32Music::Download(const std::string& song_name, const std::string& artist_name) {
    last_downloaded_data_.clear();
    current_song_name_ = song_name;

#if !CONFIG_USE_MUSIC_PLAYER
    ESP_LOGW(TAG, "CONFIG_USE_MUSIC_PLAYER is disabled");
    return false;
#else
    std::string base = CONFIG_MUSIC_GENER_BASE_URL;
    std::string full_url = base + "/api/device/play-resolve?song=" + url_encode(song_name);
    if (!artist_name.empty()) {
        full_url += "&artist=" + url_encode(artist_name);
    }

    ESP_LOGI(TAG, "play-resolve: %s", full_url.c_str());

    is_downloading_ = true;
    Application::GetInstance().Schedule([]() { Application::GetInstance().DismissChatForMusic(); });

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    http->SetHeader("User-Agent", "ESP32-Xiaozhi-Music/1.0");
    http->SetHeader("Accept", "application/json");
    http->SetHeader("X-Device-Id", SystemInfo::GetMacAddress());

    if (!http->Open("GET", full_url)) {
        ESP_LOGE(TAG, "play-resolve connect failed");
        is_downloading_ = false;
        return false;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "play-resolve HTTP %d", http->GetStatusCode());
        http->Close();
        is_downloading_ = false;
        return false;
    }
    last_downloaded_data_ = http->ReadAll();
    http->Close();

    if (!is_downloading_.load()) {
        ESP_LOGI(TAG, "play-resolve aborted");
        return false;
    }

    bool started = ApplyResolveJson(last_downloaded_data_);
    if (!started) {
        is_downloading_ = false;
    }
    return started;
#endif
}

bool Esp32Music::ApplyResolveJson(const std::string& json) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) {
        ESP_LOGE(TAG, "resolve JSON parse failed");
        return false;
    }

    cJSON* ok = cJSON_GetObjectItem(root, "ok");
    if (!cJSON_IsTrue(ok)) {
        cJSON* msg = cJSON_GetObjectItem(root, "message");
        ESP_LOGE(TAG, "resolve failed: %s", cJSON_IsString(msg) ? msg->valuestring : "?");
        cJSON_Delete(root);
        return false;
    }

    cJSON* stream_url = cJSON_GetObjectItem(root, "stream_url");
    cJSON* title = cJSON_GetObjectItem(root, "title");
    if (!cJSON_IsString(stream_url) || !stream_url->valuestring || !stream_url->valuestring[0]) {
        ESP_LOGE(TAG, "resolve missing stream_url");
        cJSON_Delete(root);
        return false;
    }
    if (cJSON_IsString(title) && title->valuestring) {
        current_song_name_ = title->valuestring;
    }
    current_music_url_ = stream_url->valuestring;

    cJSON* id_item = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsNumber(id_item)) {
        current_music_id_ = static_cast<int32_t>(id_item->valuedouble);
    } else {
        current_music_id_ = 0;
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Resolved '%s' -> %s", current_song_name_.c_str(), current_music_url_.c_str());
    return StartStreaming(current_music_url_);
}

std::string Esp32Music::GetDownloadResult() {
    return last_downloaded_data_;
}

bool Esp32Music::Pause() {
    if (!is_playing_.load()) {
        ESP_LOGW(TAG, "Pause ignored: not playing");
        return false;
    }
    is_paused_ = true;
    ESP_LOGI(TAG, "Music paused (id=%d)", (int)current_music_id_);
    auto display = Board::GetInstance().GetDisplay();
    if (display && !current_song_name_.empty()) {
        std::string status = "《" + current_song_name_ + "》已暂停";
        display->SetStatus(status.c_str());
    }
    return true;
}

bool Esp32Music::Resume() {
    if (!is_playing_.load()) {
        ESP_LOGW(TAG, "Resume ignored: not playing");
        return false;
    }
    if (!is_paused_.load()) {
        ESP_LOGW(TAG, "Resume ignored: not paused");
        return false;
    }
    is_paused_ = false;
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec && !codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    ESP_LOGI(TAG, "Music resumed (id=%d)", (int)current_music_id_);
    auto display = Board::GetInstance().GetDisplay();
    if (display && !current_song_name_.empty()) {
        std::string status = "《" + current_song_name_ + "》播放中";
        display->SetStatus(status.c_str());
    }
    return true;
}

bool Esp32Music::FetchAdjacent(const char* dir) {
#if !CONFIG_USE_MUSIC_PLAYER
    return false;
#else
    if (current_music_id_ <= 0) {
        ESP_LOGW(TAG, "adjacent: no current id");
        return false;
    }
    std::string base = CONFIG_MUSIC_GENER_BASE_URL;
    std::string full_url = base + "/api/device/adjacent?id=" +
                           std::to_string(current_music_id_) + "&dir=" + dir;
    ESP_LOGI(TAG, "adjacent: %s", full_url.c_str());

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    http->SetHeader("User-Agent", "ESP32-Xiaozhi-Music/1.0");
    http->SetHeader("Accept", "application/json");
    http->SetHeader("X-Device-Id", SystemInfo::GetMacAddress());

    if (!http->Open("GET", full_url)) {
        ESP_LOGE(TAG, "adjacent connect failed");
        return false;
    }
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "adjacent HTTP %d", http->GetStatusCode());
        http->Close();
        return false;
    }
    last_downloaded_data_ = http->ReadAll();
    http->Close();
    return ApplyResolveJson(last_downloaded_data_);
#endif
}

bool Esp32Music::Next() { return FetchAdjacent("next"); }
bool Esp32Music::Prev() { return FetchAdjacent("prev"); }

bool Esp32Music::StartStreaming(const std::string& music_url) {
    if (music_url.empty()) {
        ESP_LOGE(TAG, "Music URL is empty");
        return false;
    }

    ESP_LOGI(TAG, "Starting streaming for URL: %s", music_url.c_str());

    is_playing_ = false;
    is_downloading_ = true;
    Application::GetInstance().Schedule([]() { Application::GetInstance().DismissChatForMusic(); });

    if (download_thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        download_thread_.join();
    }
    if (play_thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }
        play_thread_.join();
    }

    ClearAudioBuffer();
    id3_bytes_remaining_ = 0;
    is_paused_ = false;

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 24576;
    cfg.prio = 5;
    cfg.thread_name = "audio_stream";
    esp_pthread_set_cfg(&cfg);

    is_downloading_ = true;
    download_thread_ = std::thread(&Esp32Music::DownloadAudioStream, this, music_url);

    cfg.thread_name = "audio_play";
    cfg.stack_size = 24576;
    cfg.prio = 5;
    esp_pthread_set_cfg(&cfg);

    is_playing_ = true;
    play_thread_ = std::thread(&Esp32Music::PlayAudioStream, this);

    ESP_LOGI(TAG, "Streaming threads started");
    return true;
}

bool Esp32Music::StopStreaming() {
    ESP_LOGI(TAG, "Stopping music streaming - downloading=%d, playing=%d",
             is_downloading_.load(), is_playing_.load());

    // Always clear track state so stop → next/prev fails even after natural end.
    is_paused_ = false;
    current_music_id_ = 0;

    if (!is_playing_ && !is_downloading_) {
        return true;
    }

    is_downloading_ = false;
    is_playing_ = false;

    auto display = Board::GetInstance().GetDisplay();
    if (display) {
        display->SetStatus("");
    }

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    if (download_thread_.joinable()) {
        download_thread_.join();
        ESP_LOGI(TAG, "Download thread joined");
    }

    if (play_thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_cv_.notify_all();
        }

        int wait_count = 0;
        const int max_wait = 100;
        while (play_thread_.joinable() && wait_count < max_wait) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
            if (!play_thread_.joinable()) {
                break;
            }
        }

        if (play_thread_.joinable()) {
            if (wait_count >= max_wait) {
                ESP_LOGW(TAG, "Play thread join timeout, detaching");
                play_thread_.detach();
            } else {
                play_thread_.join();
                ESP_LOGI(TAG, "Play thread joined");
            }
        }
    }

    ClearAudioBuffer();
    ESP_LOGI(TAG, "Music streaming stopped");
    return true;
}

void Esp32Music::DownloadAudioStream(const std::string& music_url) {
    ESP_LOGI(TAG, "Starting audio stream download");

    if (music_url.empty() || music_url.find("http") != 0) {
        ESP_LOGE(TAG, "Invalid URL format: %s", music_url.c_str());
        is_downloading_ = false;
        return;
    }

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);

    http->SetHeader("User-Agent", "ESP32-Xiaozhi-Music/1.0");
    http->SetHeader("Accept", "*/*");

    if (!http->Open("GET", music_url)) {
        ESP_LOGE(TAG, "Failed to connect to music stream URL");
        is_downloading_ = false;
        return;
    }

    int status_code = http->GetStatusCode();
    if (status_code != 200 && status_code != 206) {
        ESP_LOGE(TAG, "HTTP GET failed with status code: %d", status_code);
        http->Close();
        is_downloading_ = false;
        return;
    }

    ESP_LOGI(TAG, "Started downloading audio stream, status: %d", status_code);

    const size_t chunk_size = 4096;
    char buffer[chunk_size];
    size_t total_downloaded = 0;

    while (is_downloading_ && is_playing_) {
        int bytes_read = http->Read(buffer, chunk_size);
        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Failed to read audio data: %d", bytes_read);
            break;
        }
        if (bytes_read == 0) {
            ESP_LOGI(TAG, "Audio stream download completed, total: %u bytes",
                     (unsigned)total_downloaded);
            break;
        }

        if (total_downloaded == 0 && bytes_read >= 4) {
            if (memcmp(buffer, "ID3", 3) == 0) {
                ESP_LOGI(TAG, "Detected MP3 file with ID3 tag");
            } else if (static_cast<unsigned char>(buffer[0]) == 0xFF &&
                       (static_cast<unsigned char>(buffer[1]) & 0xE0) == 0xE0) {
                ESP_LOGI(TAG, "Detected MP3 file header");
            } else {
                ESP_LOGI(TAG, "Unknown audio format, first 4 bytes: %02X %02X %02X %02X",
                         (unsigned char)buffer[0], (unsigned char)buffer[1],
                         (unsigned char)buffer[2], (unsigned char)buffer[3]);
            }
        }

        uint8_t* chunk_data =
            (uint8_t*)heap_caps_malloc(bytes_read, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!chunk_data) {
            chunk_data = (uint8_t*)heap_caps_malloc(bytes_read, MALLOC_CAP_8BIT);
        }
        if (!chunk_data) {
            ESP_LOGE(TAG, "Failed to allocate memory for audio chunk");
            break;
        }
        memcpy(chunk_data, buffer, bytes_read);

        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            buffer_cv_.wait(lock, [this] {
                return buffer_size_ < MAX_BUFFER_SIZE || !is_downloading_;
            });

            if (is_downloading_) {
                audio_buffer_.push(AudioChunk(chunk_data, bytes_read));
                buffer_size_ += bytes_read;
                total_downloaded += bytes_read;
                buffer_cv_.notify_one();

                if (total_downloaded % (256 * 1024) == 0) {
                    ESP_LOGI(TAG, "Downloaded %u bytes, buffer size: %u",
                             (unsigned)total_downloaded, (unsigned)buffer_size_);
                }
            } else {
                heap_caps_free(chunk_data);
                break;
            }
        }
    }

    http->Close();
    is_downloading_ = false;

    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_cv_.notify_all();
    }

    ESP_LOGI(TAG, "Audio stream download thread finished");
}

void Esp32Music::PlayAudioStream() {
    ESP_LOGI(TAG, "Starting audio stream playback");

    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGE(TAG, "Audio codec not available");
        is_playing_ = false;
        return;
    }
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    const int output_rate = codec->output_sample_rate();
    esp_ae_rate_cvt_handle_t rate_cvt = nullptr;
    int opened_src_rate = 0;

    if (!mp3_decoder_initialized_) {
        ESP_LOGE(TAG, "MP3 decoder not initialized");
        is_playing_ = false;
        return;
    }

    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        buffer_cv_.wait(lock, [this] {
            return buffer_size_ >= MIN_BUFFER_SIZE || (!is_downloading_ && !audio_buffer_.empty());
        });
    }

    ESP_LOGI(TAG, "Starting playback with buffer size: %u", (unsigned)buffer_size_);

    if (!current_song_name_.empty()) {
        auto display = Board::GetInstance().GetDisplay();
        if (display) {
            std::string status = "《" + current_song_name_ + "》播放中";
            display->SetStatus(status.c_str());
        }
    }

    size_t total_played = 0;
    uint8_t* mp3_input_buffer = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mp3_input_buffer) {
        mp3_input_buffer = (uint8_t*)heap_caps_malloc(8192, MALLOC_CAP_8BIT);
    }
    if (!mp3_input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate MP3 input buffer");
        is_playing_ = false;
        return;
    }

    int16_t* pcm_buffer = (int16_t*)heap_caps_malloc(2304 * sizeof(int16_t),
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm_buffer) {
        pcm_buffer = (int16_t*)heap_caps_malloc(2304 * sizeof(int16_t), MALLOC_CAP_8BIT);
    }
    if (!pcm_buffer) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        heap_caps_free(mp3_input_buffer);
        is_playing_ = false;
        return;
    }

    int bytes_left = 0;
    uint8_t* read_ptr = nullptr;
    bool id3_header_checked = false;
    size_t decode_error_count = 0;

    while (is_playing_) {
        auto& app = Application::GetInstance();
        DeviceState current_state = app.GetDeviceState();

        // Output only in idle (Maggotxy). Do not ToggleChatState from this
        // thread: on 2.4 that event is asynchronous and a second toggle from
        // idle would re-enter listening. Application::DismissChatForMusic()
        // is responsible for returning to idle.
        if (current_state != kDeviceStateIdle) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (is_paused_.load()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (bytes_left < 4096) {
            AudioChunk chunk;

            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                if (audio_buffer_.empty()) {
                    if (!is_downloading_) {
                        ESP_LOGI(TAG, "Playback finished, total played: %u bytes",
                                 (unsigned)total_played);
                        break;
                    }
                    buffer_cv_.wait(lock, [this] {
                        return !audio_buffer_.empty() || !is_downloading_;
                    });
                    if (audio_buffer_.empty()) {
                        continue;
                    }
                }

                chunk = audio_buffer_.front();
                audio_buffer_.pop();
                buffer_size_ -= chunk.size;
                buffer_cv_.notify_one();
            }

            if (chunk.data && chunk.size > 0) {
                if (bytes_left > 0 && read_ptr != mp3_input_buffer) {
                    memmove(mp3_input_buffer, read_ptr, bytes_left);
                }

                size_t space_available = 8192 - static_cast<size_t>(bytes_left);
                size_t copy_size = std::min(chunk.size, space_available);
                memcpy(mp3_input_buffer + bytes_left, chunk.data, copy_size);
                bytes_left += static_cast<int>(copy_size);
                read_ptr = mp3_input_buffer;

                // Put unconsumed bytes back so we never drop stream data.
                if (copy_size < chunk.size) {
                    size_t rem = chunk.size - copy_size;
                    uint8_t* rem_data =
                        (uint8_t*)heap_caps_malloc(rem, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                    if (!rem_data) {
                        rem_data = (uint8_t*)heap_caps_malloc(rem, MALLOC_CAP_8BIT);
                    }
                    if (rem_data) {
                        memcpy(rem_data, chunk.data + copy_size, rem);
                        std::lock_guard<std::mutex> lock(buffer_mutex_);
                        // Keep order: new remainder must be consumed next.
                        std::queue<AudioChunk> rest;
                        rest.push(AudioChunk(rem_data, rem));
                        while (!audio_buffer_.empty()) {
                            rest.push(audio_buffer_.front());
                            audio_buffer_.pop();
                        }
                        audio_buffer_.swap(rest);
                        buffer_size_ += rem;
                        buffer_cv_.notify_one();
                    } else {
                        ESP_LOGE(TAG, "Dropped %u stream bytes (OOM)", (unsigned)rem);
                    }
                }

                heap_caps_free(chunk.data);
            }
        }

        // Finish skipping a large ID3 tag that spans multiple fills.
        if (id3_bytes_remaining_ > 0) {
            size_t skip = std::min(id3_bytes_remaining_, static_cast<size_t>(bytes_left));
            read_ptr += skip;
            bytes_left -= static_cast<int>(skip);
            id3_bytes_remaining_ -= skip;
            if (id3_bytes_remaining_ == 0) {
                ESP_LOGI(TAG, "Finished skipping ID3 tag");
            }
            continue;
        }

        if (!id3_header_checked && bytes_left >= 10) {
            size_t id3_total = GetId3TagSize(read_ptr, bytes_left);
            id3_header_checked = true;
            if (id3_total > 0) {
                ESP_LOGI(TAG, "ID3 tag size: %u bytes", (unsigned)id3_total);
                if (id3_total <= static_cast<size_t>(bytes_left)) {
                    read_ptr += id3_total;
                    bytes_left -= static_cast<int>(id3_total);
                } else {
                    id3_bytes_remaining_ = id3_total - static_cast<size_t>(bytes_left);
                    bytes_left = 0;
                    continue;
                }
            }
        }

        if (bytes_left <= 0) {
            continue;
        }

        int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);
        if (sync_offset < 0) {
            // Keep a few bytes in case the sync word straddles the next chunk.
            if (is_downloading_ && bytes_left < 4) {
                continue;
            }
            if (bytes_left > 3) {
                ESP_LOGW(TAG, "No MP3 sync word found, keeping last 3 of %d bytes", bytes_left);
                memmove(mp3_input_buffer, read_ptr + bytes_left - 3, 3);
                bytes_left = 3;
                read_ptr = mp3_input_buffer;
            }
            continue;
        }

        if (sync_offset > 0) {
            read_ptr += sync_offset;
            bytes_left -= sync_offset;
        }

        int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);

        if (decode_result == 0) {
            decode_error_count = 0;
            MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);

            if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0) {
                ESP_LOGW(TAG, "Invalid frame info: rate=%d, channels=%d",
                         mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                continue;
            }

            if (mp3_frame_info_.outputSamps > 0) {
                int16_t* final_pcm_data = pcm_buffer;
                int final_sample_count = mp3_frame_info_.outputSamps;
                std::vector<int16_t> mono_buffer;

                if (mp3_frame_info_.nChans == 2) {
                    int mono_samples = mp3_frame_info_.outputSamps / 2;
                    mono_buffer.resize(mono_samples);
                    for (int i = 0; i < mono_samples; ++i) {
                        int left = pcm_buffer[i * 2];
                        int right = pcm_buffer[i * 2 + 1];
                        mono_buffer[i] = static_cast<int16_t>((left + right) / 2);
                    }
                    final_pcm_data = mono_buffer.data();
                    final_sample_count = mono_samples;
                }

                std::vector<int16_t> resampled_buffer;
                if (!ResampleMusicPcm(&rate_cvt, &opened_src_rate, mp3_frame_info_.samprate,
                                      output_rate, final_pcm_data, final_sample_count,
                                      resampled_buffer)) {
                    ESP_LOGW(TAG, "Music resample failed, dropping frame");
                    continue;
                }
                if (resampled_buffer.empty()) {
                    continue;
                }

                if (!codec->output_enabled()) {
                    codec->EnableOutput(true);
                }
                codec->OutputData(resampled_buffer);
                Application::GetInstance().GetAudioService().NoteExternalOutput();
                size_t frame_bytes = resampled_buffer.size() * sizeof(int16_t);
                bool first_audio = (total_played == 0);
                total_played += frame_bytes;

                if (first_audio || total_played % (128 * 1024) < frame_bytes) {
                    ESP_LOGI(TAG, "Played %u bytes, src=%d Hz out=%d Hz ch=%d buffer=%u",
                             (unsigned)total_played, mp3_frame_info_.samprate, output_rate,
                             mp3_frame_info_.nChans, (unsigned)buffer_size_);
                }
            }
        } else {
            decode_error_count++;
            if (decode_error_count <= 5 || (decode_error_count % 50) == 0) {
                ESP_LOGW(TAG, "MP3 decode failed with error: %d (count=%u)",
                         decode_result, (unsigned)decode_error_count);
            }
            if (bytes_left > 1) {
                read_ptr++;
                bytes_left--;
            } else {
                bytes_left = 0;
            }
        }
    }

    heap_caps_free(pcm_buffer);
    heap_caps_free(mp3_input_buffer);
    if (rate_cvt != nullptr) {
        esp_ae_rate_cvt_close(rate_cvt);
        rate_cvt = nullptr;
    }
    is_playing_ = false;

    auto display = Board::GetInstance().GetDisplay();
    if (display) {
        display->SetStatus("");
    }

    ESP_LOGI(TAG, "Audio stream playback finished, total played: %u bytes", (unsigned)total_played);
}

void Esp32Music::ClearAudioBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    while (!audio_buffer_.empty()) {
        AudioChunk chunk = audio_buffer_.front();
        audio_buffer_.pop();
        if (chunk.data) {
            heap_caps_free(chunk.data);
        }
    }
    buffer_size_ = 0;
}

bool Esp32Music::InitializeMp3Decoder() {
    mp3_decoder_ = MP3InitDecoder();
    if (mp3_decoder_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
        mp3_decoder_initialized_ = false;
        return false;
    }
    mp3_decoder_initialized_ = true;
    ESP_LOGI(TAG, "MP3 decoder initialized");
    return true;
}

void Esp32Music::CleanupMp3Decoder() {
    if (mp3_decoder_ != nullptr) {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
    }
    mp3_decoder_initialized_ = false;
}

size_t Esp32Music::GetId3TagSize(const uint8_t* data, size_t size) {
    if (!data || size < 10) {
        return 0;
    }
    if (memcmp(data, "ID3", 3) != 0) {
        return 0;
    }
    uint32_t tag_size = ((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) |
                        ((data[8] & 0x7F) << 7) | (data[9] & 0x7F);
    // Synchsafe size excludes the 10-byte header.
    return 10 + static_cast<size_t>(tag_size);
}
