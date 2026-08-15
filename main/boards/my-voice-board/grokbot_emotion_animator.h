#ifndef GROKBOT_EMOTION_ANIMATOR_H
#define GROKBOT_EMOTION_ANIMATOR_H

#include <lvgl.h>

enum class GrokbotEmotionId {
    kNeutral,
    kHappy,
    kLaughing,
    kFunny,
    kSad,
    kAngry,
    kCrying,
    kLoving,
    kEmbarrassed,
    kSurprised,
    kShocked,
    kThinking,
    kWinking,
    kCool,
    kRelaxed,
    kDelicious,
    kKissy,
    kConfident,
    kSleepy,
    kSilly,
    kConfused,
};

// Circular GrokBot-inspired face for my-voice-board (replaces GIF at integration).
class GrokbotEmotionAnimator {
public:
    GrokbotEmotionAnimator() = default;
    ~GrokbotEmotionAnimator();

    GrokbotEmotionAnimator(const GrokbotEmotionAnimator&) = delete;
    GrokbotEmotionAnimator& operator=(const GrokbotEmotionAnimator&) = delete;

    void Create(lv_obj_t* parent);
    void SetEmotionName(const char* name);
    void SetEmotion(GrokbotEmotionId id);
    bool IsCreated() const { return face_ != nullptr; }

private:
    static GrokbotEmotionId MapName(const char* name);
    void StopAnims();
    void ResetParts();
    void ApplyEmotion(GrokbotEmotionId id);

    static void AnimHeightCb(void* var, int32_t v);
    static void AnimTranslateYCb(void* var, int32_t v);
    static void AnimTranslateXCb(void* var, int32_t v);
    static void AnimWidthCb(void* var, int32_t v);

    lv_obj_t* face_ = nullptr;
    lv_obj_t* eye_left_ = nullptr;
    lv_obj_t* eye_right_ = nullptr;
    lv_obj_t* mouth_ = nullptr;
    lv_obj_t* brow_left_ = nullptr;
    lv_obj_t* brow_right_ = nullptr;
    lv_obj_t* cheek_left_ = nullptr;
    lv_obj_t* cheek_right_ = nullptr;
    lv_obj_t* tear_ = nullptr;
    // Optional text/icon accents (hearts / zzz / ?) as lv_label children when needed.
    lv_obj_t* accent_label_ = nullptr;
};

#endif  // GROKBOT_EMOTION_ANIMATOR_H
