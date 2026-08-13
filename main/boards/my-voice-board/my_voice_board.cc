#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
#include "display/lcd_display.h"
#include "backlight.h"
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#else
#include "display/oled_display.h"
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif
#endif
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#if CONFIG_USE_MUSIC_PLAYER
#include "esp32_music.h"
#include "mcp_server.h"
#endif
#include <wifi_manager.h>
#include <esp_log.h>
#include <algorithm>
#include <string>

#define TAG "MyVoiceBoard"

class MyVoiceBoard : public WifiBoard {
private:
#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
    // Display* (not LcdDisplay*) so soft-fail can assign NoDisplay.
    Display* display_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
#else
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
#endif
#if CONFIG_USE_MUSIC_PLAYER
    Esp32Music music_;
#endif

#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        if (esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io) != ESP_OK) {
            ESP_LOGW(TAG, "Display unavailable (panel IO), continuing without LCD");
            display_ = new NoDisplay();
            return;
        }

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        if (esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel) != ESP_OK) {
            ESP_LOGW(TAG, "Display unavailable (st7789 create), continuing without LCD");
            esp_lcd_panel_io_del(panel_io);
            display_ = new NoDisplay();
            return;
        }

        if (esp_lcd_panel_reset(panel) != ESP_OK ||
            esp_lcd_panel_init(panel) != ESP_OK ||
            esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR) != ESP_OK ||
            esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY) != ESP_OK ||
            esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y) != ESP_OK ||
            esp_lcd_panel_disp_on_off(panel, true) != ESP_OK) {
            ESP_LOGW(TAG, "Display unavailable (st7789 init), continuing without LCD");
            esp_lcd_panel_del(panel);
            esp_lcd_panel_io_del(panel_io);
            display_ = new NoDisplay();
            return;
        }

        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "ST7789 %dx%d ready", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }
#else
    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void UseNoDisplay(const char* reason) {
        ESP_LOGW(TAG, "Display unavailable (%s), continuing without OLED", reason);
        if (panel_) {
            esp_lcd_panel_del(panel_);
            panel_ = nullptr;
        }
        if (panel_io_) {
            esp_lcd_panel_io_del(panel_io_);
            panel_io_ = nullptr;
        }
        display_ = new NoDisplay();
    }

    void InitializeSsd1306Display() {
        // Flaky OLED wiring can ACK-hang I2C and freeze boot before WiFi/wake.
        // Probe first with a short timeout; on any failure fall back to NoDisplay.
        constexpr uint8_t kOledAddr = 0x3C;
        esp_err_t probe = i2c_master_probe(display_i2c_bus_, kOledAddr, 100);
        if (probe != ESP_OK) {
            UseNoDisplay("I2C probe failed");
            return;
        }

        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = kOledAddr,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            // 100kHz is more tolerant of marginal Dupont / breadboard contacts
            .scl_speed_hz = 100 * 1000,
        };

        if (esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_) != ESP_OK) {
            UseNoDisplay("panel IO create failed");
            return;
        }

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        esp_err_t panel_err = esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_);
#else
        esp_err_t panel_err = esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_);
#endif
        if (panel_err != ESP_OK) {
            UseNoDisplay("panel create failed");
            return;
        }
        ESP_LOGI(TAG, "SSD1306 driver installed");

        if (esp_lcd_panel_reset(panel_) != ESP_OK || esp_lcd_panel_init(panel_) != ESP_OK) {
            UseNoDisplay("panel reset/init failed");
            return;
        }

        // Workaround for 0.91" SSD1306 128x32 panels: the driver's default
        // MULTIPLEX=0x1F (32-row addressing) and COMPINS=0x02 (sequential COM
        // pin config) leaves content squeezed in the middle of the display.
        // This particular 0.91" panel is wired in a non-standard "128x64-style"
        // COM arrangement, so it needs to be driven as if it were 64 rows even
        // though it physically has only 32. Override both:
        //   - MULTIPLEX=0x3F  (chip scans all 64 COM slots)
        //   - COMPINS=0x12    (alternative COM pin config used for 64-row panels)
        if (esp_lcd_panel_io_tx_param(panel_io_, 0xA8, (uint8_t[]){0x3F}, 1) != ESP_OK ||
            esp_lcd_panel_io_tx_param(panel_io_, 0xDA, (uint8_t[]){0x12}, 1) != ESP_OK ||
            esp_lcd_panel_invert_color(panel_, false) != ESP_OK ||
            esp_lcd_panel_disp_on_off(panel_, true) != ESP_OK) {
            UseNoDisplay("panel config failed");
            return;
        }

        ESP_LOGI(TAG, "Turning display on");
        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }
#endif

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnLongPress([this]() {
            EnterWifiConfigMode();
        });

#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = std::min(codec->output_volume() + 10, 100);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string("音量: ") + std::to_string(volume));
        });
        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = std::max(codec->output_volume() - 10, 0);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string("音量: ") + std::to_string(volume));
        });
#endif
    }

#if CONFIG_USE_MUSIC_PLAYER
    void InitializeMusicTools() {
        auto* music = GetMusic();
        if (!music) {
            return;
        }
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.music.play_song",
            "播放指定的歌曲。用户要求播放音乐、点歌时必须使用此工具。\n"
            "参数:\n"
            "  song_name: 歌曲名称（必需）\n"
            "  artist_name: 歌手（可选，默认空）\n"
            "返回: 播放是否开始；无需向用户二次确认，立即播放。"
            "用户说开始播放且当前是暂停状态时，应优先调用 self.music.resume。",
            PropertyList({
                Property("song_name", kPropertyTypeString),
                Property("artist_name", kPropertyTypeString, std::string("")),
            }),
            [music](const PropertyList& properties) -> ReturnValue {
                auto song_name = properties["song_name"].value<std::string>();
                auto artist_name = properties["artist_name"].value<std::string>();
                if (!music->Download(song_name, artist_name)) {
                    return "{\"success\": false, \"message\": \"获取或播放音乐失败\"}";
                }
                return "{\"success\": true, \"message\": \"音乐开始播放\"}";
            });

        mcp_server.AddTool("self.music.stop",
            "停止当前音乐播放。用户说停止播放、关闭音乐时使用。",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                music->StopStreaming();
                return "{\"success\": true, \"message\": \"已停止播放\"}";
            });

        mcp_server.AddTool("self.music.pause",
            "暂停当前音乐播放（保持进度，可继续）。用户说暂停、先停一下时使用。",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                if (!music->Pause()) {
                    return "{\"success\": false, \"message\": \"当前没有正在播放的音乐\"}";
                }
                return "{\"success\": true, \"message\": \"已暂停\"}";
            });

        mcp_server.AddTool("self.music.resume",
            "继续播放已暂停的音乐。用户说继续播放、接着放时使用。若未在播则应改用 play_song。",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                if (!music->Resume()) {
                    return "{\"success\": false, \"message\": \"没有可继续的暂停音乐\"}";
                }
                return "{\"success\": true, \"message\": \"继续播放\"}";
            });

        mcp_server.AddTool("self.music.next",
            "切换到曲库公开列表中的下一首（按 id 顺序）。须已有当前播放曲目。",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                if (!music->Next()) {
                    return "{\"success\": false, \"message\": \"无法切换下一首\"}";
                }
                return "{\"success\": true, \"message\": \"已切换下一首\"}";
            });

        mcp_server.AddTool("self.music.prev",
            "切换到曲库公开列表中的上一首（按 id 顺序）。须已有当前播放曲目。",
            PropertyList(),
            [music](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                if (!music->Prev()) {
                    return "{\"success\": false, \"message\": \"无法切换上一首\"}";
                }
                return "{\"success\": true, \"message\": \"已切换上一首\"}";
            });
    }
#endif

public:
#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
    MyVoiceBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
#if CONFIG_USE_MUSIC_PLAYER
        InitializeMusicTools();
#endif
        if (GetBacklight()) {
            GetBacklight()->RestoreBrightness();
        }
    }
#else
    MyVoiceBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
#if CONFIG_USE_MUSIC_PLAYER
        InitializeMusicTools();
#endif
    }
#endif

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

#if CONFIG_MY_VOICE_DISPLAY_SPI_LCD
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
#endif

#if CONFIG_USE_MUSIC_PLAYER
    virtual Music* GetMusic() override { return &music_; }
#endif
};

DECLARE_BOARD(MyVoiceBoard);
