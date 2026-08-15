# GrokBot Emotion Animator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a new circular GrokBot-style LVGL face animator (21 emotions) in new files, keep `EyeEmotionAnimator` untouched, then switch `MyVoiceLcdDisplay` to the new animator.

**Architecture:** `GrokbotEmotionAnimator` owns a circular face (`lv_obj`) with eyes, mouth, brows, cheeks, and optional accent props. `SetEmotionName` maps the 21 XiaoZhi emotion strings to an `EmotionId` and applies layout + `lv_anim` loops. `MyVoiceLcdDisplay` is the only integration point; board `*.cc` is already GLOB’d by `main/CMakeLists.txt`.

**Tech Stack:** ESP-IDF C++, LVGL (`lv_obj` + `lv_anim`), my-voice-board SPI LCD path.

**Spec:** `docs/superpowers/specs/2026-08-15-grokbot-emotion-animator-design.md`  
**Visual ref:** `docs/superpowers/specs/assets/2026-08-15-grokbot-face-mockup.html`

---

## File map

| File | Action |
|------|--------|
| `main/boards/my-voice-board/grokbot_emotion_animator.h` | Create |
| `main/boards/my-voice-board/grokbot_emotion_animator.cc` | Create |
| `main/boards/my-voice-board/my_voice_lcd_display.h` | Modify — use new animator type |
| `main/boards/my-voice-board/my_voice_lcd_display.cc` | Modify — construct / call new animator |
| `main/boards/my-voice-board/README.md` | Modify — note GrokBot-style face |
| `main/boards/my-voice-board/eye_emotion_animator.*` | **Do not modify** |

---

### Task 1: Header + circular face scaffold (`neutral` only)

**Files:**
- Create: `main/boards/my-voice-board/grokbot_emotion_animator.h`
- Create: `main/boards/my-voice-board/grokbot_emotion_animator.cc`

- [ ] **Step 1: Write the header**

```cpp
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
```

- [ ] **Step 2: Implement Create + destructor + anim callbacks + `neutral` blink**

In `grokbot_emotion_animator.cc`:

- Constants (portrait 240×320): `kFace = 168`, `kEye = 44`, `kEyeGap = 30`.
- Face: square `kFace×kFace`, `LV_RADIUS_CIRCLE`, bg `#F4F1EA`, transparent border/pad, centered on `parent`.
- Eyes: dark `#1A1A1E` circles; each with a small white highlight child (`lv_obj` 10×10 circle, top-left inset).
- Mouth: small arc-like object (border bottom or filled rounded rect); default neutral smile outline.
- Brows / cheeks / tear: create hidden (`LV_OBJ_FLAG_HIDDEN`) until an emotion shows them.
- `accent_label_`: empty `lv_label`, hidden by default.
- `StopAnims`: `lv_anim_del` on face, both eyes, mouth, brows.
- `ResetParts`: stop anims; restore eye size/pos/color; hide brows/cheeks/tear/accent; reset mouth to neutral geometry.
- `SetEmotion(kNeutral)`: infinite blink on both eyes (height `kEye` → `kEye/10`, 150ms, repeat delay ~2800ms) + optional gentle face scale via translate-y breathe (0 ↔ -3, ~1600ms).
- `MapName`: for Task 1, map everything unknown/`nullptr`/`""` → `kNeutral`; implement full table in Task 2.
- `SetEmotionName` → `SetEmotion(MapName(name))`.

- [ ] **Step 3: Build board to ensure new file compiles (not wired yet)**

```bash
source /path/to/esp-idf/export.sh   # Mac: /Users/ming/esp/esp-idf-v5.5.2/export.sh
cd /Users/ming/Desktop/xiaozhi-esp32-official
python3 scripts/build.py my-voice-board --name my-voice-board
```

Expected: build succeeds (GLOB picks up `grokbot_emotion_animator.cc`; display still uses old animator).

- [ ] **Step 4: Commit**

```bash
git add main/boards/my-voice-board/grokbot_emotion_animator.h \
        main/boards/my-voice-board/grokbot_emotion_animator.cc
git commit -m "$(cat <<'EOF'
feat(board): add GrokbotEmotionAnimator scaffold with neutral face

Circular LVGL face with eyes/mouth parts; old EyeEmotionAnimator untouched.
EOF
)"
```

---

### Task 2: Full `MapName` for 21 protocol names

**Files:**
- Modify: `main/boards/my-voice-board/grokbot_emotion_animator.cc`

- [ ] **Step 1: Implement complete `MapName`**

Exact mapping (strcmp):

| name | id |
|------|-----|
| `neutral` | `kNeutral` |
| `happy` | `kHappy` |
| `laughing` | `kLaughing` |
| `funny` | `kFunny` |
| `sad` | `kSad` |
| `angry` | `kAngry` |
| `crying` | `kCrying` |
| `loving` | `kLoving` |
| `embarrassed` | `kEmbarrassed` |
| `surprised` | `kSurprised` |
| `shocked` | `kShocked` |
| `thinking` | `kThinking` |
| `winking` | `kWinking` |
| `cool` | `kCool` |
| `relaxed` | `kRelaxed` |
| `delicious` | `kDelicious` |
| `kissy` | `kKissy` |
| `confident` | `kConfident` |
| `sleepy` | `kSleepy` |
| `silly` | `kSilly` |
| `confused` | `kConfused` |
| nullptr / "" / other | `kNeutral` |

- [ ] **Step 2: Commit**

```bash
git add main/boards/my-voice-board/grokbot_emotion_animator.cc
git commit -m "feat(board): map all 21 emotion names to GrokbotEmotionId"
```

---

### Task 3: Apply all 21 emotion layouts + loops

**Files:**
- Modify: `main/boards/my-voice-board/grokbot_emotion_animator.cc` (`ApplyEmotion`)

Visual source of truth: HTML mockup. On device, prefer geometry + `lv_anim` over CSS-only tricks. Use dark eyes `#1A1A1E`, face `#F4F1EA`, blush `#FDA4AF`, accents as needed.

- [ ] **Step 1: Implement `ApplyEmotion` switch for every `GrokbotEmotionId`**

For each case: call `ResetParts()` first (already done in `SetEmotion`), then set static layout, then start infinite anims.

Required look (match mockup intent):

1. **kNeutral** — blink + breathe (Task 1).
2. **kHappy** — crescent eyes (reduced height, bottom radius), wide filled smile mouth, show cheeks, bounce translate-y on face (~400ms).
3. **kLaughing** — like happy but taller mouth / stronger bounce (~250ms).
4. **kFunny** — eyes rotate via small opposite translate-x; mouth tilted ( asymmetric width); accent `★` or star via label; wiggle face translate-x.
5. **kSad** — eyes lower (translate-y +); upside-down mouth (top border / inverted radius); slow sink bounce.
6. **kAngry** — show angled brows; flatter/shorter eyes; thin mouth; shake translate-x on face (~50ms).
7. **kCrying** — sad mouth + show `tear_` with translate-y loop; soft sob bounce.
8. **kLoving** — happy-ish eyes; smile; cheeks; accent label `♥`; soft bounce.
9. **kEmbarrassed** — smaller round mouth; strong cheeks; accent sweat/`😅` optional as `•`; fidget rotate via small translate-x.
10. **kSurprised** — larger eyes (width/height bump); round O mouth; jump translate-y.
11. **kShocked** — even larger eyes; big O mouth; show raised brows; shake.
12. **kThinking** — right eye shifted up/right; tiny mouth; accent `…`; slow breathe.
13. **kWinking** — left eye blink-closed loop; right open; smile mouth.
14. **kCool** — half-lidded eyes (height ~60%); slight brows; **smirk** mouth (asymmetric, no sunglasses); slight head tilt via face translate.
15. **kRelaxed** — soft crescent eyes; gentle smile; slow breathe.
16. **kDelicious** — crescent eyes; chew mouth scale height; accent `♪`; bounce.
17. **kKissy** — crescent eyes; small round mouth tinted pink (`#FB7185`); hearts accent; kiss scale pulse on mouth.
18. **kConfident** — show slight brows; almost-full eyes; confident smile; slow nod translate-y.
19. **kSleepy** — eyes as thin lines; accent `z z`; slow breathe.
20. **kSilly** — asymmetric eye sizes/offsets; crooked mouth; wiggle; star accent.
21. **kConfused** — uneven brows; tilted small mouth; accent `?`; tilt/fidget.

Helpers allowed inside the `.cc` (anonymous namespace): `Show(obj)`, `Hide(obj)`, `StartBlink(eye)`, `StartBounce(face, amp, ms)`, `StartShake(face, amp, ms)`.

- [ ] **Step 2: Build again**

```bash
python3 scripts/build.py my-voice-board --name my-voice-board
```

Expected: success.

- [ ] **Step 3: Commit**

```bash
git add main/boards/my-voice-board/grokbot_emotion_animator.cc
git commit -m "feat(board): implement 21 GrokBot face emotion layouts and loops"
```

---

### Task 4: Swap `MyVoiceLcdDisplay` integration

**Files:**
- Modify: `main/boards/my-voice-board/my_voice_lcd_display.h`
- Modify: `main/boards/my-voice-board/my_voice_lcd_display.cc`

- [ ] **Step 1: Update header includes and member type**

Replace:

```cpp
#include "eye_emotion_animator.h"
// ...
std::unique_ptr<EyeEmotionAnimator> eye_animator_;
```

With:

```cpp
#include "grokbot_emotion_animator.h"
// ...
std::unique_ptr<GrokbotEmotionAnimator> face_animator_;
```

Update class comment to mention GrokBot-style circular face.

- [ ] **Step 2: Update `.cc` to construct and drive the new animator**

- `EnsureAnimator`: `make_unique<GrokbotEmotionAnimator>()`, `Create(emoji_box_)`.
- `SetupUI`: after ensure, `face_animator_->SetEmotion(GrokbotEmotionId::kNeutral)` (or `SetEmotionName("neutral")`).
- `SetEmotion`: call `face_animator_->SetEmotionName(emotion)` after hiding GIF/label/image (same as today).
- Keep hiding `emoji_label_` / `emoji_image_` / stopping GIF.

Do **not** remove `eye_emotion_animator.*` from the tree (still GLOB-compiled; harmless).

- [ ] **Step 3: Build**

```bash
python3 scripts/build.py my-voice-board --name my-voice-board
```

Expected: success; firmware now shows new face when emotions fire.

- [ ] **Step 4: Commit**

```bash
git add main/boards/my-voice-board/my_voice_lcd_display.h \
        main/boards/my-voice-board/my_voice_lcd_display.cc
git commit -m "feat(board): wire MyVoiceLcdDisplay to GrokbotEmotionAnimator"
```

---

### Task 5: README + verification notes

**Files:**
- Modify: `main/boards/my-voice-board/README.md` (Emotions section)

- [ ] **Step 1: Document**

Replace GIF/eye-animator blurb with: chat face uses LVGL `GrokbotEmotionAnimator` (circular face, 21 names); visual ref link to the HTML mockup; old `EyeEmotionAnimator` retained unused in tree for revert.

- [ ] **Step 2: Commit**

```bash
git add main/boards/my-voice-board/README.md
git commit -m "docs(board): document GrokBot-style chat face animator"
```

- [ ] **Step 3: Hardware smoke (when Pi/USB available)**

Flash `my-voice-board`, then verify:

1. Idle home: face hidden.
2. Enter chat / wake: circular face visible, `neutral` blinks.
3. Force several `SetEmotion` names via conversation (happy/sad/angry/cool/sleepy at minimum).
4. Confirm no GIF emoji flashing; volume/audio unrelated.

Report what was tested vs still needs hardware.

---

## Spec coverage check

| Spec item | Task |
|-----------|------|
| New files, keep old animator | 1–3 keep old; 1 creates new |
| Circle face, eyes+mouth+accents | 1, 3 |
| 21 names | 2, 3 |
| `cool` without sunglasses | 3 (`kCool`) |
| Swap only display wiring | 4 |
| README | 5 |
| Build my-voice-board | 1, 3, 4 |

## Revert

Point `my_voice_lcd_display.*` back to `EyeEmotionAnimator` / `eye_animator_`. Leave `grokbot_emotion_animator.*` in tree or delete in a follow-up.
