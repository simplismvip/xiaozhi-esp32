#ifndef MY_VOICE_LCD_DISPLAY_H
#define MY_VOICE_LCD_DISPLAY_H

#include "display/lcd_display.h"
#include "grokbot_emotion_animator.h"

#include <memory>

// SpiLcdDisplay subclass: GrokBot-style circular face instead of GIF/Noto emoji for chat face.
class MyVoiceLcdDisplay : public SpiLcdDisplay {
public:
    MyVoiceLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                      int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                      bool swap_xy);

    void SetupUI() override;
    void SetEmotion(const char* emotion) override;

private:
    void EnsureAnimator();

    std::unique_ptr<GrokbotEmotionAnimator> face_animator_;
};

#endif  // MY_VOICE_LCD_DISPLAY_H
