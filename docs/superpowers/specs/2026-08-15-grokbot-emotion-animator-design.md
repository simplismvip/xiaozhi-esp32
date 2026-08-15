# GrokBot-style Face Emotions for my-voice-board

Date: 2026-08-15  
Status: Approved (approach A + keep old animator files)  
Visual reference: `docs/superpowers/specs/assets/2026-08-15-grokbot-face-mockup.html`

## Goal

Ship a **new** LVGL face animator that matches the approved circular GrokBot-inspired mockup (eyes + mouth + light accents, 21 protocol emotion names with looping motion). Keep the existing five-eye `EyeEmotionAnimator` sources on disk. When the new animator is ready, **swap only the board display wiring** so chat emotions use the new face.

## Non-goals

- Do not delete or refactor `eye_emotion_animator.h` / `.cc` in this work.
- Do not change Application, protocol emotion name list, or idle-home layout.
- Do not embed the HTML mockup in firmware; it is visual reference only.
- Do not add a runtime dual-skin toggle (user chose replace-at-integration, not menuconfig dual mode).

## Decisions

| Topic | Choice |
|-------|--------|
| Approach | A — full 21-name face with parts + animation table |
| Old code | **Keep** `EyeEmotionAnimator` files unchanged |
| New code | New files under `main/boards/my-voice-board/` |
| Integration | `MyVoiceLcdDisplay` switches from old animator to new |
| Face shape | Circle (not oval) |
| Parts | Eyes + mouth required; brows / cheeks / tear / hearts as accents when needed |
| `cool` | Half-lidded eyes + smirk (no sunglasses), per mockup revision |
| Mapping | One protocol name → one face style (not collapsed to 5 bases) |

## Emotion names (unchanged protocol set)

`neutral` `happy` `laughing` `funny` `sad` `angry` `crying` `loving` `embarrassed` `surprised` `shocked` `thinking` `winking` `cool` `relaxed` `delicious` `kissy` `confident` `sleepy` `silly` `confused`

Unknown / non-face icons → `neutral`.

## Architecture

```text
SetEmotion(name)
    → MyVoiceLcdDisplay (lock, hide GIF/label)
    → GrokbotEmotionAnimator::SetEmotionName(name)
        → look up EmotionSpec
        → reset parts + start lv_anim loops
```

### New files

| File | Role |
|------|------|
| `grokbot_emotion_animator.h` | Public API: `Create`, `SetEmotionName`, `IsCreated`; private part handles + spec apply |
| `grokbot_emotion_animator.cc` | Circle face UI, per-emotion layout + `lv_anim` loops aligned to mockup |

### Unchanged (retained)

| File | Role |
|------|------|
| `eye_emotion_animator.h` / `.cc` | Previous 5-base eye-only animator (kept for reference / easy revert) |

### Wiring change (final step only)

| File | Change |
|------|--------|
| `my_voice_lcd_display.h` | Hold `std::unique_ptr<GrokbotEmotionAnimator>` instead of `EyeEmotionAnimator` |
| `my_voice_lcd_display.cc` | Construct / `Create` / `SetEmotionName` on the new type; default emotion `neutral` |
| `main/CMakeLists.txt` (or board source glob) | Ensure new `.cc` is compiled with my-voice-board |

Hide `emoji_label_` / `emoji_image_` / GIF path remains as today. Parent remains `emoji_box_`. Idle `SetHomeVisible(true)` continues to hide the emoji box.

## Visual / motion

- Portrait 240×320: face diameter ~160–200 px class, centered in `emoji_box_`.
- Cream circular face; dark pupils with highlight; simple mouth shapes.
- Each of the 21 names has a distinct static layout **and** a looping motion (blink, bounce, shake, tear, etc.), matching the HTML mockup intent. Exact timing may be simplified for LVGL/CPU, but silhouettes and accent props should read the same.
- Prefer opaque `lv_obj` geometry over bitmap assets (no new GIF/PNG pack required).

## Implementation order

1. Add `grokbot_emotion_animator.*` with `Create` + `neutral` loop working on device/simulator path.
2. Implement remaining 20 emotion specs + animations (can land in one PR or staged commits).
3. Switch `MyVoiceLcdDisplay` to the new animator; build `my-voice-board`.
4. Hardware smoke: chat `SetEmotion` for several names; idle still hides face; no GIF regression.

## Revert path

Point `MyVoiceLcdDisplay` back at `EyeEmotionAnimator` and drop the new `.cc` from the build list. Old sources remain in tree.

## Testing

- Build: `python3 scripts/build.py my-voice-board`
- On device: trigger emotions via normal chat / `SetEmotion`; confirm circular face and motion.
- Confirm unknown emotion names fall back to `neutral`.
- Confirm home idle hides face; leaving idle shows animator again.
