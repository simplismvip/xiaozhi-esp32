#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "gif/lvgl_gif.h"
#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define PREVIEW_IMAGE_DURATION_MS 5000

class LvglTheme;

class LcdDisplay : public LvglDisplay {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_draw_buf_t draw_buf_;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* bottom_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_ = nullptr;
    lv_obj_t* emoji_box_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    esp_timer_handle_t preview_timer_ = nullptr;
    std::unique_ptr<LvglImage> preview_image_cached_ = nullptr;
    bool hide_subtitle_ = false;  // Control whether to hide chat messages/subtitles

    lv_obj_t* home_panel_ = nullptr;
    lv_obj_t* home_time_label_ = nullptr;
    lv_obj_t* home_date_label_ = nullptr;
    lv_obj_t* home_weather_label_ = nullptr;
    lv_obj_t* home_temp_label_ = nullptr;
    lv_obj_t* home_humidity_label_ = nullptr;
    lv_obj_t* home_wake_hint_label_ = nullptr;
    bool home_visible_ = false;

    lv_obj_t* now_playing_panel_ = nullptr;
    lv_obj_t* now_playing_title_label_ = nullptr;
    lv_obj_t* now_playing_artist_label_ = nullptr;
    lv_obj_t* now_playing_lyrics_label_ = nullptr;
    lv_obj_t* now_playing_prev_label_ = nullptr;
    lv_obj_t* now_playing_play_label_ = nullptr;
    lv_obj_t* now_playing_next_label_ = nullptr;
    lv_obj_t* now_playing_bar_ = nullptr;
    lv_obj_t* now_playing_elapsed_label_ = nullptr;
    lv_obj_t* now_playing_total_label_ = nullptr;
    bool now_playing_visible_ = false;
    bool now_playing_has_lrc_ = false;
    std::string now_playing_plain_lyrics_;
    std::vector<std::pair<int, std::string>> now_playing_lrc_;
    int now_playing_duration_ms_ = 0;

    void CreateNowPlayingPanel(LvglTheme* theme, const lv_font_t* text_font);
    void ParseNowPlayingLyrics(const std::string& lyrics);
    void UpdateNowPlayingLyrics(int position_ms);
    void ApplyNowPlayingProgress(int position_ms, int duration_ms, bool paused);

    void InitializeLcdThemes();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // Add protected constructor
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
               int height);

public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void ClearChatMessages() override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void SetupUI() override;
    // Add theme switching function
    virtual void SetTheme(Theme* theme) override;

    virtual void SetHomeVisible(bool visible) override;
    virtual bool IsHomeVisible() const override { return home_visible_; }
    virtual void SetHomeEnvironment(const char* weather_text, const char* temp_text,
                                    const char* humidity_text) override;
    virtual void SetHomeClock(const char* time_text, const char* date_text) override;

    virtual void SetNowPlayingVisible(bool visible) override;
    virtual bool IsNowPlayingVisible() const override { return now_playing_visible_; }
    virtual void SetNowPlaying(const NowPlayingInfo& info) override;
    virtual void SetNowPlayingProgress(int position_ms, int duration_ms, bool paused) override;

    // Set whether to hide chat messages/subtitles
    void SetHideSubtitle(bool hide);
};

// SPI LCD display
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                  int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                  bool swap_xy);
};

// RGB LCD display
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                  int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                  bool swap_xy);
};

// MIPI LCD display
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                   int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                   bool swap_xy);
};

#endif  // LCD_DISPLAY_H
