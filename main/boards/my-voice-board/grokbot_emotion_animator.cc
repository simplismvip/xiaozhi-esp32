#include "grokbot_emotion_animator.h"

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
    lv_obj_set_height(static_cast<lv_obj_t*>(var), v);
}

void GrokbotEmotionAnimator::AnimTranslateYCb(void* var, int32_t v) {
    lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(var), v, 0);
}

void GrokbotEmotionAnimator::AnimTranslateXCb(void* var, int32_t v) {
    lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(var), v, 0);
}

void GrokbotEmotionAnimator::AnimWidthCb(void* var, int32_t v) {
    lv_obj_set_width(static_cast<lv_obj_t*>(var), v);
}

GrokbotEmotionId GrokbotEmotionAnimator::MapName(const char* name) {
    (void)name;
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
    lv_obj_set_style_bg_color(eye_left_, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_bg_color(eye_right_, lv_color_hex(kColorInk), 0);

    if (face_ != nullptr) {
        lv_obj_set_style_translate_y(face_, 0, 0);
    }

    if (mouth_ != nullptr) {
        SetNeutralMouthStyle(mouth_);
        lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, kMouthYOffset);
        lv_obj_clear_flag(mouth_, LV_OBJ_FLAG_HIDDEN);
    }

    if (brow_left_ != nullptr) {
        lv_obj_add_flag(brow_left_, LV_OBJ_FLAG_HIDDEN);
    }
    if (brow_right_ != nullptr) {
        lv_obj_add_flag(brow_right_, LV_OBJ_FLAG_HIDDEN);
    }
    if (cheek_left_ != nullptr) {
        lv_obj_add_flag(cheek_left_, LV_OBJ_FLAG_HIDDEN);
    }
    if (cheek_right_ != nullptr) {
        lv_obj_add_flag(cheek_right_, LV_OBJ_FLAG_HIDDEN);
    }
    if (tear_ != nullptr) {
        lv_obj_add_flag(tear_, LV_OBJ_FLAG_HIDDEN);
    }
    if (accent_label_ != nullptr) {
        lv_label_set_text(accent_label_, "");
        lv_obj_add_flag(accent_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void GrokbotEmotionAnimator::ApplyEmotion(GrokbotEmotionId id) {
    lv_anim_t a_h;
    lv_anim_t a_y;
    lv_anim_init(&a_h);
    lv_anim_init(&a_y);

    const int32_t eye = kEye;
    const int32_t blink_min = eye / 10;

    switch (id) {
        case GrokbotEmotionId::kNeutral:
        default:
            lv_anim_set_exec_cb(&a_h, AnimHeightCb);
            lv_anim_set_values(&a_h, eye, blink_min);
            lv_anim_set_time(&a_h, 150);
            lv_anim_set_playback_time(&a_h, 150);
            lv_anim_set_repeat_delay(&a_h, 2800);
            lv_anim_set_repeat_count(&a_h, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_var(&a_h, eye_left_);
            lv_anim_start(&a_h);
            lv_anim_set_var(&a_h, eye_right_);
            lv_anim_start(&a_h);

            lv_anim_set_exec_cb(&a_y, AnimTranslateYCb);
            lv_anim_set_values(&a_y, 0, -3);
            lv_anim_set_time(&a_y, 1600);
            lv_anim_set_playback_time(&a_y, 1600);
            lv_anim_set_repeat_count(&a_y, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a_y, lv_anim_path_ease_in_out);
            lv_anim_set_var(&a_y, face_);
            lv_anim_start(&a_y);
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
    lv_obj_add_flag(brow_left_, LV_OBJ_FLAG_HIDDEN);

    brow_right_ = lv_obj_create(face_);
    lv_obj_set_size(brow_right_, 24, 4);
    lv_obj_set_style_border_width(brow_right_, 0, 0);
    lv_obj_set_style_bg_color(brow_right_, lv_color_hex(kColorInk), 0);
    lv_obj_set_style_radius(brow_right_, 4, 0);
    lv_obj_align(brow_right_, LV_ALIGN_TOP_MID, 28, 48);
    lv_obj_add_flag(brow_right_, LV_OBJ_FLAG_HIDDEN);

    cheek_left_ = lv_obj_create(face_);
    lv_obj_set_size(cheek_left_, 14, 8);
    lv_obj_set_style_border_width(cheek_left_, 0, 0);
    lv_obj_set_style_bg_color(cheek_left_, lv_color_hex(kColorCheek), 0);
    lv_obj_set_style_radius(cheek_left_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(cheek_left_, LV_ALIGN_CENTER, -42, 18);
    lv_obj_add_flag(cheek_left_, LV_OBJ_FLAG_HIDDEN);

    cheek_right_ = lv_obj_create(face_);
    lv_obj_set_size(cheek_right_, 14, 8);
    lv_obj_set_style_border_width(cheek_right_, 0, 0);
    lv_obj_set_style_bg_color(cheek_right_, lv_color_hex(kColorCheek), 0);
    lv_obj_set_style_radius(cheek_right_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(cheek_right_, LV_ALIGN_CENTER, 42, 18);
    lv_obj_add_flag(cheek_right_, LV_OBJ_FLAG_HIDDEN);

    tear_ = lv_obj_create(face_);
    lv_obj_set_size(tear_, 8, 12);
    lv_obj_set_style_border_width(tear_, 0, 0);
    lv_obj_set_style_bg_color(tear_, lv_color_hex(kColorTear), 0);
    lv_obj_set_style_radius(tear_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(tear_, LV_ALIGN_CENTER, -22, 24);
    lv_obj_add_flag(tear_, LV_OBJ_FLAG_HIDDEN);

    accent_label_ = lv_label_create(face_);
    lv_label_set_text(accent_label_, "");
    lv_obj_add_flag(accent_label_, LV_OBJ_FLAG_HIDDEN);

    SetEmotion(GrokbotEmotionId::kNeutral);
}
