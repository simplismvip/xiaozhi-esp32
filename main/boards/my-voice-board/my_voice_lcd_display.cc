#include "my_voice_lcd_display.h"

#include <esp_log.h>

#define TAG "MyVoiceLcdDisplay"

MyVoiceLcdDisplay::MyVoiceLcdDisplay(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_handle_t panel, int width, int height,
                                     int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                                     bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y,
                    swap_xy) {}

void MyVoiceLcdDisplay::EnsureAnimator() {
    if (eye_animator_ == nullptr) {
        eye_animator_ = std::make_unique<EyeEmotionAnimator>();
    }
    if (!eye_animator_->IsCreated() && emoji_box_ != nullptr) {
        eye_animator_->Create(emoji_box_);
    }
}

void MyVoiceLcdDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();

    DisplayLockGuard lock(this);
    EnsureAnimator();
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }
    if (eye_animator_ != nullptr) {
        eye_animator_->SetEmotion(EyeEmotionType::kNormalBlink);
    }
}

void MyVoiceLcdDisplay::SetEmotion(const char* emotion) {
    if (!IsSetupUICalled()) {
        ESP_LOGW(TAG, "SetEmotion('%s') called before SetupUI()", emotion ? emotion : "");
        return;
    }

    DisplayLockGuard lock(this);
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }

    EnsureAnimator();
    if (eye_animator_ != nullptr && eye_animator_->IsCreated()) {
        eye_animator_->SetEmotionName(emotion);
    }
}
