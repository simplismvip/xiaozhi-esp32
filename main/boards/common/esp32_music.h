#ifndef ESP32_MUSIC_H
#define ESP32_MUSIC_H

#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "music.h"

extern "C" {
#include "mp3dec.h"
}

struct AudioChunk {
    uint8_t* data;
    size_t size;

    AudioChunk() : data(nullptr), size(0) {}
    AudioChunk(uint8_t* d, size_t s) : data(d), size(s) {}
};

class Esp32Music : public Music {
private:
    std::string last_downloaded_data_;
    std::string current_music_url_;
    std::string current_song_name_;

    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::atomic<bool> is_paused_{false};
    int32_t current_music_id_ = 0;
    std::thread play_thread_;
    std::thread download_thread_;

    std::queue<AudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    size_t buffer_size_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;

    HMP3Decoder mp3_decoder_;
    MP3FrameInfo mp3_frame_info_;
    bool mp3_decoder_initialized_;
    // Large ID3 tags (album art) often exceed one buffer fill; skip across chunks.
    size_t id3_bytes_remaining_ = 0;

    void DownloadAudioStream(const std::string& music_url);
    void PlayAudioStream();
    void ClearAudioBuffer();
    bool InitializeMp3Decoder();
    void CleanupMp3Decoder();
    // Returns full ID3 tag length including 10-byte header, or 0 if no ID3 / too short.
    size_t GetId3TagSize(const uint8_t* data, size_t size);
    // Returns false on parse/play failure. Sets current_music_id_ / song name / url.
    bool ApplyResolveJson(const std::string& json);
    bool FetchAdjacent(const char* dir);  // "next" or "prev"

public:
    Esp32Music();
    ~Esp32Music();

    bool Download(const std::string& song_name, const std::string& artist_name = "") override;
    std::string GetDownloadResult() override;
    bool StartStreaming(const std::string& music_url) override;
    bool StopStreaming() override;
    bool IsPlaying() const override { return is_playing_.load(); }
    bool IsActive() const override { return is_playing_.load() || is_downloading_.load(); }
    bool Pause() override;
    bool Resume() override;
    bool Next() override;
    bool Prev() override;
    int32_t CurrentMusicId() const override { return current_music_id_; }
};

#endif  // ESP32_MUSIC_H
