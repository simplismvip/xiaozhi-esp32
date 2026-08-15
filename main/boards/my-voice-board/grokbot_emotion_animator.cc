#include "grokbot_emotion_animator.h"

#include <cstring>

namespace {

// Portrait 240x320 layout constants.
constexpr lv_coord_t kFace = 168;
constexpr lv_coord_t kEye = 44;
constexpr lv_coord_t kEyeGap = 30;

constexpr lv_coord_t kMouthNeutralW = 18;
constexpr lv_coord_t kMouthNeutralH = 5;
constexpr lv_coord_t kEyeYOffset = -18;
constexpr lv_coord_t kMouthYOffset = 32;

constexpr uint32_t kColorFace = 0xF4F1EA;
constexpr uint32_t kColorInk = 0x1A1A1E;
constexpr uint32_t kColorCheek = 0xFDA4AF;
constexpr uint32_t kColorTear = 0x7DD3FC;
constexpr uint32_t kColorKiss = 0xFB7185;
constexpr uint32_t kColorAccent = 0x94A3B8;

// LVGL transform_angle uses 0.1 deg units.
constexpr int16_t Deg(int16_t degrees) {
    return static_cast<int16_t>(degrees * 10);
}

void Show(lv_obj_t* obj) {
    if (obj != nullptr) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void Hide(lv_obj_t* obj) {
    if (obj != nullptr) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void HeightCb(void* var, int32_t v) {
    lv_obj_set_height(static_cast<lv_obj_t*>(var), v);
}

void WidthCb(void* var, int32_t v) {
    lv_obj_set_width(static_cast<lv_obj_t*>(var), v);
}

void TranslateYCb(void* var, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(var), v, 0);
}

void TranslateXCb(void* var, int32_t v) {
    lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(var), v, 0);
}

void StartBlink(lv_obj_t* eye, int32_t open_h, int32_t closed_h, uint32_t close_ms, uint32_t delay_ms) {
    if (eye == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, HeightCb);
    lv_anim_set_values(&a, open_h, closed_h);
    lv_anim_set_time(&a, close_ms);
    lv_anim_set_playback_time(&a, close_ms);
    lv_anim_set_repeat_delay(&a, delay_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_var(&a, eye);
    lv_anim_start(&a);
}

void StartBounce(lv_obj_t* obj, int32_t amp, uint32_t ms) {
    if (obj == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, TranslateYCb);
    lv_anim_set_values(&a, 0, -amp);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_var(&a, obj);
    lv_anim_start(&a);
}

void StartShake(lv_obj_t* obj, int32_t amp, uint32_t ms) {
    if (obj == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, TranslateXCb);
    lv_anim_set_values(&a, -amp, amp);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_var(&a, obj);
    lv_anim_start(&a);
}

void StartSink(lv_obj_t* obj, int32_t amp, uint32_t ms) {
    if (obj == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, TranslateYCb);
    lv_anim_set_values(&a, 0, amp);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_var(&a, obj);
    lv_anim_start(&a);
}

void StartHeightPulse(lv_obj_t* obj, int32_t from, int32_t to, uint32_t ms) {
    if (obj == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, HeightCb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_var(&a, obj);
    lv_anim_start(&a);
}

void StartWidthPulse(lv_obj_t* obj, int32_t from, int32_t to, uint32_t ms) {
    if (obj == nullptr) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, WidthCb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_set_playback_time(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_var(&a, obj);
    lv_anim_start(&a);
}

void SetAccent(lv_obj_t* label, const char* text, lv_coord_t x_off, lv_coord_t y_off) {
    if (label == nullptr || text == nullptr) {
        return;
    }
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(kColorAccent), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, x_off, y_off);
}

void ShowCheeks(lv_obj_t* left, lv_obj_t* right, lv_coord_t w = 14, lv_coord_t h = 8) {
    if (left != nullptr) {
        lv_obj_set_size(left, w, h);
        Show(left);
    }
    if (right != nullptr) {
        lv_obj_set_size(right, w, h);
        Show(right);
    }
}

void SetCrescentEyes(lv_obj_t* left, lv_obj_t* right, lv_coord_t h, lv_coord_t y_nudge) {
    if (left != nullptr) {
        lv_obj_set_size(left, kEye, h);
        lv_obj_set_style_radius(left, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_translate_y(left, y_nudge, 0);
    }
    if (right != nullptr) {
        lv_obj_set_size(right, kEye, h);
        lv_obj_set_style_radius(right, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_translate_y(right, y_nudge, 0);
    }
}

void SetFilledSmile(lv_obj_t* mouth, lv_coord_t w, lv_coord_t h) {
    if (mouth == nullptr) {
        return;
    }
    lv_obj_set_size(mouth, w, h);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(mouth, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_border_width(mouth, 0, 0);
    lv_obj_set_style_radius(mouth, h, 0);
    lv_obj_set_style_transform_angle(mouth, 0, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, kMouthYOffset);
    Show(mouth);
}

void SetNeutralMouthStyle(lv_obj_t* mouth) {
    lv_obj_set_size(mouth, kMouthNeutralW, kMouthNeutralH);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mouth, 2, 0);
    lv_obj_set_style_border_color(mouth, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_border_side(
        mouth,
        static_cast<lv_border_side_t>(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM),
        0);
    lv_obj_set_style_radius(mouth, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_translate_x(mouth, 0, 0);
    lv_obj_set_style_translate_y(mouth, 0, 0);
    lv_obj_set_style_transform_angle(mouth, 0, 0);
}

void SetSadMouth(lv_obj_t* mouth, lv_coord_t w, lv_coord_t h) {
    if (mouth == nullptr) {
        return;
    }
    lv_obj_set_size(mouth, w, h);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mouth, 3, 0);
    lv_obj_set_style_border_color(mouth, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_border_side(
        mouth,
        static_cast<lv_border_side_t>(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_TOP),
        0);
    lv_obj_set_style_radius(mouth, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, kMouthYOffset);
    Show(mouth);
}

void SetRoundMouth(lv_obj_t* mouth, lv_coord_t size, bool filled, uint32_t fill_color) {
    if (mouth == nullptr) {
        return;
    }
    lv_obj_set_size(mouth, size, size);
    lv_obj_set_style_radius(mouth, LV_RADIUS_CIRCLE, 0);
    if (filled) {
        lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(mouth, lv_color_hex(fill_color), 0);
        lv_obj_set_style_border_width(mouth, 0, 0);
    } else {
        lv_obj_set_style_bg_opa(mouth, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(mouth, 3, 0);
        lv_obj_set_style_border_color(mouth, lv_color_hex(kColorInk), 0);
        lv_obj_set_style_border_side(mouth, LV_BORDER_SIDE_FULL, 0);
    }
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, kMouthYOffset);
    Show(mouth);
}

void SetLineMouth(lv_obj_t* mouth, lv_coord_t w, lv_coord_t h) {
    if (mouth == nullptr) {
        return;
    }
    lv_obj_set_size(mouth, w, h);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(mouth, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_border_width(mouth, 0, 0);
    lv_obj_set_style_radius(mouth, 4, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, kMouthYOffset);
    Show(mouth);
}

void SetSmirkMouth(lv_obj_t* mouth) {
    if (mouth == nullptr) {
        return;
    }
    lv_obj_set_size(mouth, 26, 10);
    lv_obj_set_style_bg_opa(mouth, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mouth, 3, 0);
    lv_obj_set_style_border_color(mouth, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_border_side(
        mouth,
        static_cast<lv_border_side_t>(LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM),
        0);
    lv_obj_set_style_radius(mouth, 14, 0);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 6, kMouthYOffset);
    Show(mouth);
}

void ShowBrows(lv_obj_t* left, lv_obj_t* right, int16_t left_deg, int16_t right_deg, lv_coord_t y) {
    if (left != nullptr) {
        lv_obj_set_style_transform_angle(left, Deg(left_deg), 0);
        lv_obj_align(left, LV_ALIGN_TOP_MID, -28, y);
        Show(left);
    }
    if (right != nullptr) {
        lv_obj_set_style_transform_angle(right, Deg(right_deg), 0);
        lv_obj_align(right, LV_ALIGN_TOP_MID, 28, y);
        Show(right);
    }
}

}  // namespace

GrokbotEmotionAnimator::~GrokbotEmotionAnimator() {
    if (face_ != nullptr) {
        StopAnims();
        lv_obj_del(face_);
        face_ = nullptr;
        eye_left_ = nullptr;
        eye_right_ = nullptr;
        mouth_ = nullptr;
        brow_left_ = nullptr;
        brow_right_ = nullptr;
        cheek_left_ = nullptr;
        cheek_right_ = nullptr;
        tear_ = nullptr;
        accent_label_ = nullptr;
    }
}

void GrokbotEmotionAnimator::AnimHeightCb(void* var, int32_t v) {
    HeightCb(var, v);
}

void GrokbotEmotionAnimator::AnimTranslateYCb(void* var, int32_t v) {
    TranslateYCb(var, v);
}

void GrokbotEmotionAnimator::AnimTranslateXCb(void* var, int32_t v) {
    TranslateXCb(var, v);
}

void GrokbotEmotionAnimator::AnimWidthCb(void* var, int32_t v) {
    WidthCb(var, v);
}

GrokbotEmotionId GrokbotEmotionAnimator::MapName(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return GrokbotEmotionId::kNeutral;
    }

    if (strcmp(name, "neutral") == 0) {
        return GrokbotEmotionId::kNeutral;
    }
    if (strcmp(name, "happy") == 0) {
        return GrokbotEmotionId::kHappy;
    }
    if (strcmp(name, "laughing") == 0) {
        return GrokbotEmotionId::kLaughing;
    }
    if (strcmp(name, "funny") == 0) {
        return GrokbotEmotionId::kFunny;
    }
    if (strcmp(name, "sad") == 0) {
        return GrokbotEmotionId::kSad;
    }
    if (strcmp(name, "angry") == 0) {
        return GrokbotEmotionId::kAngry;
    }
    if (strcmp(name, "crying") == 0) {
        return GrokbotEmotionId::kCrying;
    }
    if (strcmp(name, "loving") == 0) {
        return GrokbotEmotionId::kLoving;
    }
    if (strcmp(name, "embarrassed") == 0) {
        return GrokbotEmotionId::kEmbarrassed;
    }
    if (strcmp(name, "surprised") == 0) {
        return GrokbotEmotionId::kSurprised;
    }
    if (strcmp(name, "shocked") == 0) {
        return GrokbotEmotionId::kShocked;
    }
    if (strcmp(name, "thinking") == 0) {
        return GrokbotEmotionId::kThinking;
    }
    if (strcmp(name, "winking") == 0) {
        return GrokbotEmotionId::kWinking;
    }
    if (strcmp(name, "cool") == 0) {
        return GrokbotEmotionId::kCool;
    }
    if (strcmp(name, "relaxed") == 0) {
        return GrokbotEmotionId::kRelaxed;
    }
    if (strcmp(name, "delicious") == 0) {
        return GrokbotEmotionId::kDelicious;
    }
    if (strcmp(name, "kissy") == 0) {
        return GrokbotEmotionId::kKissy;
    }
    if (strcmp(name, "confident") == 0) {
        return GrokbotEmotionId::kConfident;
    }
    if (strcmp(name, "sleepy") == 0) {
        return GrokbotEmotionId::kSleepy;
    }
    if (strcmp(name, "silly") == 0) {
        return GrokbotEmotionId::kSilly;
    }
    if (strcmp(name, "confused") == 0) {
        return GrokbotEmotionId::kConfused;
    }

    return GrokbotEmotionId::kNeutral;
}

void GrokbotEmotionAnimator::StopAnims() {
    if (face_ != nullptr) {
        lv_anim_del(face_, nullptr);
    }
    if (eye_left_ != nullptr) {
        lv_anim_del(eye_left_, nullptr);
    }
    if (eye_right_ != nullptr) {
        lv_anim_del(eye_right_, nullptr);
    }
    if (mouth_ != nullptr) {
        lv_anim_del(mouth_, nullptr);
    }
    if (brow_left_ != nullptr) {
        lv_anim_del(brow_left_, nullptr);
    }
    if (brow_right_ != nullptr) {
        lv_anim_del(brow_right_, nullptr);
    }
    if (tear_ != nullptr) {
        lv_anim_del(tear_, nullptr);
    }
    if (cheek_left_ != nullptr) {
        lv_anim_del(cheek_left_, nullptr);
    }
    if (cheek_right_ != nullptr) {
        lv_anim_del(cheek_right_, nullptr);
    }
    if (accent_label_ != nullptr) {
        lv_anim_del(accent_label_, nullptr);
    }
}

void GrokbotEmotionAnimator::ResetParts() {
    StopAnims();

    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }

    lv_obj_set_size(eye_left_, kEye, kEye);
    lv_obj_set_size(eye_right_, kEye, kEye);
    lv_obj_align(eye_left_, LV_ALIGN_CENTER, -kEyeGap, kEyeYOffset);
    lv_obj_align(eye_right_, LV_ALIGN_CENTER, kEyeGap, kEyeYOffset);
    lv_obj_set_style_translate_x(eye_left_, 0, 0);
    lv_obj_set_style_translate_x(eye_right_, 0, 0);
    lv_obj_set_style_translate_y(eye_left_, 0, 0);
    lv_obj_set_style_translate_y(eye_right_, 0, 0);
    lv_obj_set_style_radius(eye_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(eye_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(eye_left_, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_hex(kColorInk), 0);

    if (face_ != nullptr) {
        lv_obj_set_style_translate_x(face_, 0, 0);
        lv_obj_set_style_translate_y(face_, 0, 0);
        lv_obj_set_style_transform_angle(face_, 0, 0);
    }

    if (mouth_ != nullptr) {
        SetNeutralMouthStyle(mouth_);
        lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, kMouthYOffset);
        Show(mouth_);
    }

    if (brow_left_ != nullptr) {
        lv_obj_set_style_transform_angle(brow_left_, 0, 0);
        lv_obj_set_style_translate_y(brow_left_, 0, 0);
        lv_obj_align(brow_left_, LV_ALIGN_TOP_MID, -28, 48);
        Hide(brow_left_);
    }
    if (brow_right_ != nullptr) {
        lv_obj_set_style_transform_angle(brow_right_, 0, 0);
        lv_obj_set_style_translate_y(brow_right_, 0, 0);
        lv_obj_align(brow_right_, LV_ALIGN_TOP_MID, 28, 48);
        Hide(brow_right_);
    }
    if (cheek_left_ != nullptr) {
        lv_obj_set_size(cheek_left_, 14, 8);
        Hide(cheek_left_);
    }
    if (cheek_right_ != nullptr) {
        lv_obj_set_size(cheek_right_, 14, 8);
        Hide(cheek_right_);
    }
    if (tear_ != nullptr) {
        lv_obj_set_style_translate_y(tear_, 0, 0);
        Hide(tear_);
    }
    if (accent_label_ != nullptr) {
        lv_label_set_text(accent_label_, "");
        Hide(accent_label_);
    }
}

void GrokbotEmotionAnimator::ApplyEmotion(GrokbotEmotionId id) {
    const int32_t eye = kEye;
    const int32_t blink_min = eye / 10;

    switch (id) {
        case GrokbotEmotionId::kNeutral:
        default:
            StartBlink(eye_left_, eye, blink_min, 150, 2800);
            StartBlink(eye_right_, eye, blink_min, 150, 2800);
            StartBounce(face_, 3, 1600);
            break;

        case GrokbotEmotionId::kHappy:
            SetCrescentEyes(eye_left_, eye_right_, 18, 6);
            SetFilledSmile(mouth_, 44, 22);
            ShowCheeks(cheek_left_, cheek_right_);
            StartBounce(face_, 6, 400);
            StartHeightPulse(eye_left_, 18, 14, 600);
            StartHeightPulse(eye_right_, 18, 14, 600);
            break;

        case GrokbotEmotionId::kLaughing:
            SetCrescentEyes(eye_left_, eye_right_, 14, 8);
            SetFilledSmile(mouth_, 52, 28);
            ShowCheeks(cheek_left_, cheek_right_);
            StartBounce(face_, 10, 250);
            StartHeightPulse(eye_left_, 14, 10, 275);
            StartHeightPulse(eye_right_, 14, 10, 275);
            break;

        case GrokbotEmotionId::kFunny:
            lv_obj_set_style_translate_x(eye_left_, -3, 0);
            lv_obj_set_style_translate_x(eye_right_, 3, 0);
            SetFilledSmile(mouth_, 40, 18);
            lv_obj_set_style_transform_angle(mouth_, Deg(8), 0);
            ShowCheeks(cheek_left_, cheek_right_);
            SetAccent(accent_label_, "\xE2\x98\x85", -8, 18);  // ★
            StartShake(face_, 4, 350);
            StartShake(eye_left_, 2, 275);
            StartShake(eye_right_, 2, 275);
            break;

        case GrokbotEmotionId::kSad:
            lv_obj_set_height(eye_left_, eye * 60 / 100);
            lv_obj_set_height(eye_right_, eye * 60 / 100);
            lv_obj_set_style_translate_y(eye_left_, 6, 0);
            lv_obj_set_style_translate_y(eye_right_, 6, 0);
            SetSadMouth(mouth_, 28, 12);
            StartSink(face_, 6, 1000);
            StartSink(eye_left_, 4, 1000);
            StartSink(eye_right_, 4, 1000);
            break;

        case GrokbotEmotionId::kAngry: {
            const int32_t angry_h = eye * 45 / 100;
            lv_obj_set_height(eye_left_, angry_h);
            lv_obj_set_height(eye_right_, angry_h);
            ShowBrows(brow_left_, brow_right_, 18, -18, 46);
            SetLineMouth(mouth_, 32, 4);
            StartShake(face_, 3, 50);
            break;
        }

        case GrokbotEmotionId::kCrying:
            lv_obj_set_height(eye_left_, 36);
            lv_obj_set_height(eye_right_, 36);
            SetSadMouth(mouth_, 30, 14);
            Show(tear_);
            StartSink(face_, 4, 550);
            StartSink(tear_, 10, 550);
            StartHeightPulse(eye_left_, 36, 30, 550);
            StartHeightPulse(eye_right_, 36, 30, 550);
            break;

        case GrokbotEmotionId::kLoving:
            SetFilledSmile(mouth_, 40, 18);
            ShowCheeks(cheek_left_, cheek_right_);
            SetAccent(accent_label_, "\xE2\x99\xA5", -6, 16);  // ♥
            lv_obj_set_style_text_color(accent_label_, lv_color_hex(kColorKiss), 0);
            StartBlink(eye_left_, eye, blink_min, 150, 2200);
            StartBlink(eye_right_, eye, blink_min, 150, 2200);
            StartBounce(face_, 5, 700);
            break;

        case GrokbotEmotionId::kEmbarrassed:
            lv_obj_set_height(eye_left_, 42);
            lv_obj_set_height(eye_right_, 42);
            SetRoundMouth(mouth_, 14, false, kColorInk);
            ShowCheeks(cheek_left_, cheek_right_, 18, 12);
            SetAccent(accent_label_, "\xE2\x80\xA2", -4, 20);  // •
            StartShake(face_, 3, 750);
            StartHeightPulse(eye_left_, 42, 38, 750);
            StartHeightPulse(eye_right_, 42, 38, 750);
            break;

        case GrokbotEmotionId::kSurprised:
            lv_obj_set_size(eye_left_, 54, 54);
            lv_obj_set_size(eye_right_, 54, 54);
            SetRoundMouth(mouth_, 22, true, kColorInk);
            StartBounce(face_, 8, 650);
            StartWidthPulse(eye_left_, 54, 50, 650);
            StartWidthPulse(eye_right_, 54, 50, 650);
            break;

        case GrokbotEmotionId::kShocked:
            lv_obj_set_size(eye_left_, 56, 56);
            lv_obj_set_size(eye_right_, 56, 56);
            SetRoundMouth(mouth_, 28, true, kColorInk);
            ShowBrows(brow_left_, brow_right_, -12, 12, 42);
            StartShake(face_, 4, 45);
            break;

        case GrokbotEmotionId::kThinking:
            lv_obj_set_style_translate_x(eye_right_, 4, 0);
            lv_obj_set_style_translate_y(eye_right_, -4, 0);
            SetRoundMouth(mouth_, 12, false, kColorInk);
            SetAccent(accent_label_, "...", -10, 22);
            StartBounce(face_, 3, 1400);
            break;

        case GrokbotEmotionId::kWinking:
            SetFilledSmile(mouth_, 34, 14);
            StartBlink(eye_left_, eye, blink_min, 120, 1500);
            // Keep right eye mostly open with a soft pulse.
            StartHeightPulse(eye_right_, eye, eye - 4, 900);
            StartBounce(face_, 4, 900);
            break;

        case GrokbotEmotionId::kCool:
            lv_obj_set_height(eye_left_, eye * 60 / 100);
            lv_obj_set_height(eye_right_, eye * 60 / 100);
            ShowBrows(brow_left_, brow_right_, -6, 6, 52);
            SetSmirkMouth(mouth_);
            lv_obj_set_style_translate_x(face_, 2, 0);
            StartHeightPulse(eye_left_, eye * 60 / 100, eye * 55 / 100, 1100);
            StartHeightPulse(eye_right_, eye * 60 / 100, eye * 55 / 100, 1100);
            StartBounce(face_, 2, 1100);
            break;

        case GrokbotEmotionId::kRelaxed:
            SetCrescentEyes(eye_left_, eye_right_, 22, 4);
            SetFilledSmile(mouth_, 30, 10);
            StartBounce(face_, 3, 1700);
            StartHeightPulse(eye_left_, 22, 18, 1500);
            StartHeightPulse(eye_right_, 22, 18, 1500);
            break;

        case GrokbotEmotionId::kDelicious:
            SetCrescentEyes(eye_left_, eye_right_, 16, 6);
            SetFilledSmile(mouth_, 36, 20);
            ShowCheeks(cheek_left_, cheek_right_);
            SetAccent(accent_label_, "\xE2\x99\xAA", -8, 18);  // ♪
            lv_obj_set_style_text_color(accent_label_, lv_color_hex(0xFBBF24), 0);
            StartBounce(face_, 6, 450);
            StartHeightPulse(mouth_, 20, 14, 450);
            break;

        case GrokbotEmotionId::kKissy:
            SetCrescentEyes(eye_left_, eye_right_, 20, 6);
            SetRoundMouth(mouth_, 16, true, kColorKiss);
            lv_obj_set_style_border_width(mouth_, 2, 0);
            lv_obj_set_style_border_color(mouth_, lv_color_hex(kColorInk), 0);
            lv_obj_set_style_border_side(mouth_, LV_BORDER_SIDE_FULL, 0);
            SetAccent(accent_label_, "\xE2\x99\xA5", -6, 16);  // ♥
            lv_obj_set_style_text_color(accent_label_, lv_color_hex(kColorKiss), 0);
            StartBounce(face_, 5, 550);
            StartWidthPulse(mouth_, 16, 20, 550);
            StartHeightPulse(mouth_, 14, 18, 550);
            break;

        case GrokbotEmotionId::kConfident:
            ShowBrows(brow_left_, brow_right_, -8, 8, 48);
            SetFilledSmile(mouth_, 38, 12);
            StartBlink(eye_left_, eye, blink_min, 150, 2400);
            StartBlink(eye_right_, eye, blink_min, 150, 2400);
            StartBounce(face_, 5, 1100);
            break;

        case GrokbotEmotionId::kSleepy:
            lv_obj_set_size(eye_left_, kEye, 4);
            lv_obj_set_size(eye_right_, kEye, 4);
            lv_obj_set_style_radius(eye_left_, 4, 0);
            lv_obj_set_style_radius(eye_right_, 4, 0);
            SetNeutralMouthStyle(mouth_);
            lv_obj_set_size(mouth_, 28, 8);
            lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, kMouthYOffset);
            SetAccent(accent_label_, "z z", -8, 14);
            StartBounce(face_, 3, 1400);
            StartHeightPulse(eye_left_, 4, 2, 1200);
            StartHeightPulse(eye_right_, 4, 2, 1200);
            break;

        case GrokbotEmotionId::kSilly:
            lv_obj_set_size(eye_left_, 36, 40);
            lv_obj_set_size(eye_right_, 48, 36);
            lv_obj_set_style_translate_y(eye_left_, -4, 0);
            lv_obj_set_style_translate_y(eye_right_, 4, 0);
            SetFilledSmile(mouth_, 42, 16);
            lv_obj_set_style_transform_angle(mouth_, Deg(-10), 0);
            SetAccent(accent_label_, "\xE2\x98\x85", -8, 18);  // ★
            StartShake(face_, 5, 350);
            break;

        case GrokbotEmotionId::kConfused:
            ShowBrows(brow_left_, brow_right_, -16, 6, 46);
            lv_obj_set_style_translate_y(brow_right_, 4, 0);
            SetRoundMouth(mouth_, 18, false, kColorInk);
            lv_obj_set_style_transform_angle(mouth_, Deg(12), 0);
            SetAccent(accent_label_, "?", -6, 18);
            StartShake(face_, 3, 700);
            StartBounce(face_, 2, 700);
            break;
    }
}

void GrokbotEmotionAnimator::SetEmotionName(const char* name) {
    SetEmotion(MapName(name));
}

void GrokbotEmotionAnimator::SetEmotion(GrokbotEmotionId id) {
    if (!IsCreated()) {
        return;
    }

    ResetParts();
    ApplyEmotion(id);
}

void GrokbotEmotionAnimator::Create(lv_obj_t* parent) {
    if (face_ != nullptr || parent == nullptr) {
        return;
    }

    face_ = lv_obj_create(parent);
    lv_obj_set_size(face_, kFace, kFace);
    lv_obj_set_style_bg_color(face_, lv_color_hex(kColorFace), 0);
    lv_obj_set_style_radius(face_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(face_, 0, 0);
    lv_obj_set_style_pad_all(face_, 0, 0);
    lv_obj_set_scrollbar_mode(face_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(face_);

    for (int i = 0; i < 2; i++) {
        lv_obj_t* eye = lv_obj_create(face_);
        lv_obj_set_size(eye, kEye, kEye);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_bg_color(eye, lv_color_hex(kColorInk), 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
        if (i == 0) {
            lv_obj_align(eye, LV_ALIGN_CENTER, -kEyeGap, kEyeYOffset);
            eye_left_ = eye;
        } else {
            lv_obj_align(eye, LV_ALIGN_CENTER, kEyeGap, kEyeYOffset);
            eye_right_ = eye;
        }

        lv_obj_t* highlight = lv_obj_create(eye);
        lv_obj_set_size(highlight, 10, 10);
        lv_obj_set_style_border_width(highlight, 0, 0);
        lv_obj_set_style_bg_color(highlight, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(highlight, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_scrollbar_mode(highlight, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(highlight, LV_ALIGN_TOP_LEFT, 4, 4);
    }

    mouth_ = lv_obj_create(face_);
    lv_obj_set_style_pad_all(mouth_, 0, 0);
    lv_obj_set_scrollbar_mode(mouth_, LV_SCROLLBAR_MODE_OFF);
    SetNeutralMouthStyle(mouth_);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, kMouthYOffset);

    brow_left_ = lv_obj_create(face_);
    lv_obj_set_size(brow_left_, 24, 4);
    lv_obj_set_style_border_width(brow_left_, 0, 0);
    lv_obj_set_style_bg_color(brow_left_, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_radius(brow_left_, 4, 0);
    lv_obj_align(brow_left_, LV_ALIGN_TOP_MID, -28, 48);
    Hide(brow_left_);

    brow_right_ = lv_obj_create(face_);
    lv_obj_set_size(brow_right_, 24, 4);
    lv_obj_set_style_border_width(brow_right_, 0, 0);
    lv_obj_set_style_bg_color(brow_right_, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_radius(brow_right_, 4, 0);
    lv_obj_align(brow_right_, LV_ALIGN_TOP_MID, 28, 48);
    Hide(brow_right_);

    cheek_left_ = lv_obj_create(face_);
    lv_obj_set_size(cheek_left_, 14, 8);
    lv_obj_set_style_border_width(cheek_left_, 0, 0);
    lv_obj_set_style_bg_color(cheek_left_, lv_color_hex(kColorCheek), 0);
    lv_obj_set_style_radius(cheek_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(cheek_left_, LV_ALIGN_CENTER, -42, 18);
    Hide(cheek_left_);

    cheek_right_ = lv_obj_create(face_);
    lv_obj_set_size(cheek_right_, 14, 8);
    lv_obj_set_style_border_width(cheek_right_, 0, 0);
    lv_obj_set_style_bg_color(cheek_right_, lv_color_hex(kColorCheek), 0);
    lv_obj_set_style_radius(cheek_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(cheek_right_, LV_ALIGN_CENTER, 42, 18);
    Hide(cheek_right_);

    tear_ = lv_obj_create(face_);
    lv_obj_set_size(tear_, 8, 12);
    lv_obj_set_style_border_width(tear_, 0, 0);
    lv_obj_set_style_bg_color(tear_, lv_color_hex(kColorTear), 0);
    lv_obj_set_style_radius(tear_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(tear_, LV_ALIGN_CENTER, -22, 24);
    Hide(tear_);

    accent_label_ = lv_label_create(face_);
    lv_label_set_text(accent_label_, "");
    Hide(accent_label_);

    SetEmotion(GrokbotEmotionId::kNeutral);
}
