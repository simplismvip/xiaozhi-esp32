#include "eye_emotion_animator.h"

#include <cstring>

namespace {

// Portrait 240x320: scale original ~284x126 / 100px eyes down to fit emoji_box_.
constexpr lv_coord_t kFaceW = 140;
constexpr lv_coord_t kFaceH = 64;
constexpr lv_coord_t kEyeSize = 48;
constexpr lv_coord_t kEyeGap = 34;  // center offset each side

}  // namespace

EyeEmotionAnimator::~EyeEmotionAnimator() {
    if (face_bg_ != nullptr) {
        lv_anim_del(eye_left_, nullptr);
        lv_anim_del(eye_right_, nullptr);
        lv_anim_del(face_bg_, nullptr);
        lv_obj_del(face_bg_);
        face_bg_ = nullptr;
        eye_left_ = nullptr;
        eye_right_ = nullptr;
    }
}

void EyeEmotionAnimator::AnimHeightCb(void* var, int32_t v) {
    lv_obj_set_height(static_cast<lv_obj_t*>(var), v);
}

void EyeEmotionAnimator::AnimTranslateYCb(void* var, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(var), v, 0);
}

void EyeEmotionAnimator::AnimTranslateXCb(void* var, int32_t v) {
    lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(var), v, 0);
}

void EyeEmotionAnimator::Create(lv_obj_t* parent) {
    if (face_bg_ != nullptr || parent == nullptr) {
        return;
    }

    face_bg_ = lv_obj_create(parent);
    lv_obj_set_size(face_bg_, kFaceW, kFaceH);
    lv_obj_set_style_bg_opa(face_bg_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(face_bg_, 0, 0);
    lv_obj_set_style_pad_all(face_bg_, 0, 0);
    lv_obj_set_scrollbar_mode(face_bg_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(face_bg_);

    for (int i = 0; i < 2; i++) {
        lv_obj_t* eye = lv_obj_create(face_bg_);
        lv_obj_set_size(eye, kEyeSize, kEyeSize);
        lv_obj_set_style_border_width(eye, 0, 0);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
        if (i == 0) {
            lv_obj_align(eye, LV_ALIGN_CENTER, -kEyeGap, 0);
            eye_left_ = eye;
        } else {
            lv_obj_align(eye, LV_ALIGN_CENTER, kEyeGap, 0);
            eye_right_ = eye;
        }
    }

    SetEmotion(EyeEmotionType::kNormalBlink);
}

EyeEmotionType EyeEmotionAnimator::MapName(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return EyeEmotionType::kNormalBlink;
    }

    // Happy cluster (+ surprised/shocked per approved mapping)
    if (strcmp(name, "happy") == 0 || strcmp(name, "laughing") == 0 || strcmp(name, "funny") == 0 ||
        strcmp(name, "loving") == 0 || strcmp(name, "embarrassed") == 0 ||
        strcmp(name, "delicious") == 0 || strcmp(name, "kissy") == 0 ||
        strcmp(name, "confident") == 0 || strcmp(name, "surprised") == 0 ||
        strcmp(name, "shocked") == 0) {
        return EyeEmotionType::kHappy;
    }
    if (strcmp(name, "sad") == 0 || strcmp(name, "crying") == 0) {
        return EyeEmotionType::kSad;
    }
    if (strcmp(name, "angry") == 0) {
        return EyeEmotionType::kAngry;
    }
    if (strcmp(name, "sleepy") == 0) {
        return EyeEmotionType::kSleep;
    }
    // neutral / thinking / relaxed / cool / winking / confused / silly / unknown
    return EyeEmotionType::kNormalBlink;
}

void EyeEmotionAnimator::ResetEyes() {
    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }
    lv_anim_del(eye_left_, nullptr);
    lv_anim_del(eye_right_, nullptr);
    if (face_bg_ != nullptr) {
        lv_anim_del(face_bg_, nullptr);
    }

    lv_obj_set_height(eye_left_, kEyeSize);
    lv_obj_set_height(eye_right_, kEyeSize);
    lv_obj_set_style_translate_x(eye_left_, 0, 0);
    lv_obj_set_style_translate_x(eye_right_, 0, 0);
    lv_obj_set_style_translate_y(eye_left_, 0, 0);
    lv_obj_set_style_translate_y(eye_right_, 0, 0);
    lv_obj_set_style_bg_color(eye_left_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_hex(0xFFFFFF), 0);
}

void EyeEmotionAnimator::SetEmotionName(const char* name) { SetEmotion(MapName(name)); }

void EyeEmotionAnimator::SetEmotion(EyeEmotionType type) {
    if (eye_left_ == nullptr || eye_right_ == nullptr) {
        return;
    }

    ResetEyes();

    lv_anim_t a_h;
    lv_anim_t a_y;
    lv_anim_t a_x;
    lv_anim_init(&a_h);
    lv_anim_init(&a_y);
    lv_anim_init(&a_x);

    const int32_t eye = kEyeSize;
    const int32_t blink_min = eye / 10;        // ~5
    const int32_t sleep_min = eye * 15 / 100;  // ~7
    const int32_t happy_h = eye * 70 / 100;    // ~34
    const int32_t sad_h = eye * 60 / 100;      // ~29
    const int32_t angry_h = eye * 45 / 100;    // ~22

    switch (type) {
        case EyeEmotionType::kNormalBlink:
            lv_anim_set_exec_cb(&a_h, AnimHeightCb);
            lv_anim_set_values(&a_h, eye, blink_min);
            lv_anim_set_time(&a_h, 150);
            lv_anim_set_playback_time(&a_h, 150);
            lv_anim_set_repeat_delay(&a_h, 3500);
            lv_anim_set_repeat_count(&a_h, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_var(&a_h, eye_left_);
            lv_anim_start(&a_h);
            lv_anim_set_var(&a_h, eye_right_);
            lv_anim_start(&a_h);
            break;

        case EyeEmotionType::kSleep:
            lv_anim_set_exec_cb(&a_h, AnimHeightCb);
            lv_anim_set_values(&a_h, eye, sleep_min);
            lv_anim_set_time(&a_h, 1500);
            lv_anim_set_playback_time(&a_h, 1500);
            lv_anim_set_playback_delay(&a_h, 400);
            lv_anim_set_repeat_count(&a_h, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a_h, lv_anim_path_ease_in_out);
            lv_anim_set_var(&a_h, eye_left_);
            lv_anim_start(&a_h);
            lv_anim_set_var(&a_h, eye_right_);
            lv_anim_start(&a_h);
            break;

        case EyeEmotionType::kHappy:
            lv_obj_set_height(eye_left_, happy_h);
            lv_obj_set_height(eye_right_, happy_h);
            lv_anim_set_exec_cb(&a_y, AnimTranslateYCb);
            lv_anim_set_values(&a_y, 0, -12);
            lv_anim_set_time(&a_y, 400);
            lv_anim_set_playback_time(&a_y, 400);
            lv_anim_set_repeat_count(&a_y, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a_y, lv_anim_path_overshoot);
            lv_anim_set_var(&a_y, eye_left_);
            lv_anim_start(&a_y);
            lv_anim_set_var(&a_y, eye_right_);
            lv_anim_start(&a_y);
            break;

        case EyeEmotionType::kSad:
            lv_obj_set_height(eye_left_, sad_h);
            lv_obj_set_height(eye_right_, sad_h);
            lv_anim_set_exec_cb(&a_y, AnimTranslateYCb);
            lv_anim_set_values(&a_y, 0, 14);
            lv_anim_set_time(&a_y, 2000);
            lv_anim_set_playback_time(&a_y, 2000);
            lv_anim_set_repeat_count(&a_y, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a_y, lv_anim_path_ease_in_out);
            lv_anim_set_var(&a_y, eye_left_);
            lv_anim_start(&a_y);
            lv_anim_set_var(&a_y, eye_right_);
            lv_anim_start(&a_y);
            break;

        case EyeEmotionType::kAngry:
            lv_obj_set_style_bg_color(eye_left_, lv_color_hex(0xFF3333), 0);
            lv_obj_set_style_bg_color(eye_right_, lv_color_hex(0xFF3333), 0);
            lv_obj_set_height(eye_left_, angry_h);
            lv_obj_set_height(eye_right_, angry_h);
            lv_anim_set_exec_cb(&a_x, AnimTranslateXCb);
            lv_anim_set_values(&a_x, -3, 3);
            lv_anim_set_time(&a_x, 50);
            lv_anim_set_playback_time(&a_x, 50);
            lv_anim_set_repeat_count(&a_x, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_var(&a_x, eye_left_);
            lv_anim_start(&a_x);
            lv_anim_set_var(&a_x, eye_right_);
            lv_anim_start(&a_x);
            break;
    }
}
