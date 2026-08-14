# Eye Emotion Animator for my-voice-board

Date: 2026-08-14  
Branch: `feat/music-ha-mcp`  
Status: Approved

## Goal

Replace GIF/Noto emoji on `my-voice-board` chat face with LVGL vector eye animations (port of `ui_emotions`), mapped from the standard 21 XiaoZhi emotion names. Minimal core changes: board-local `SpiLcdDisplay` subclass only.

## Emotion names (protocol / noto_emoji)

`neutral` `happy` `laughing` `funny` `sad` `angry` `crying` `loving` `embarrassed` `surprised` `shocked` `thinking` `winking` `cool` `relaxed` `delicious` `kissy` `confident` `sleepy` `silly` `confused`

Non-face icons (`robot_2`, `link`, `warning`, …) → `NORMAL_BLINK`.

## Base animations → mapping

| `emotion_type_t` | Names |
|--|--|
| `NORMAL_BLINK` | `neutral`, `thinking`, `relaxed`, `cool`, `winking`, `confused`, `silly`, unknown |
| `SLEEP` | `sleepy` |
| `HAPPY` | `happy`, `laughing`, `funny`, `loving`, `embarrassed`, `delicious`, `kissy`, `confident`, `surprised`, `shocked` |
| `SAD` | `sad`, `crying` |
| `ANGRY` | `angry` |

## Architecture

- `EyeEmotionAnimator` — LVGL eyes + `lv_anim`, parent = `emoji_box_`
- `MyVoiceLcdDisplay : SpiLcdDisplay` — override `SetupUI` / `SetEmotion` only
- `my_voice_board.cc` constructs `MyVoiceLcdDisplay` instead of `SpiLcdDisplay`
- Idle `SetHomeVisible(true)` still hides `emoji_box_`; no Application changes

## Visual direction

Target look: **cute / “萌”** eye character face (soft round eyes, gentle motion) — not a 1:1 GIF clone. Phase 1 ships the five base loops; timing, size, bounce, and color can be tuned later without changing the name→type mapping.

