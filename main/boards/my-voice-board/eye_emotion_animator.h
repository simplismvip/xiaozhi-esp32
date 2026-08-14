#ifndef EYE_EMOTION_ANIMATOR_H
#define EYE_EMOTION_ANIMATOR_H

#include <lvgl.h>

enum class EyeEmotionType {
    kNormalBlink,
    kSleep,
    kHappy,
    kSad,
    kAngry,
};

// LVGL vector eye face for my-voice-board. Lightweight alternative to GIF emotions.
class EyeEmotionAnimator {
public:
    EyeEmotionAnimator() = default;
    ~EyeEmotionAnimator();

    EyeEmotionAnimator(const EyeEmotionAnimator&) = delete;
    EyeEmotionAnimator& operator=(const EyeEmotionAnimator&) = delete;

    void Create(lv_obj_t* parent);
    void SetEmotionName(const char* name);
    void SetEmotion(EyeEmotionType type);
    bool IsCreated() const { return face_bg_ != nullptr; }

private:
    static EyeEmotionType MapName(const char* name);
    void ResetEyes();

    static void AnimHeightCb(void* var, int32_t v);
    static void AnimTranslateYCb(void* var, int32_t v);
    static void AnimTranslateXCb(void* var, int32_t v);

    lv_obj_t* face_bg_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
};

#endif  // EYE_EMOTION_ANIMATOR_H
