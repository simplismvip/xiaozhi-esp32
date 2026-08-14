# Idle Home Dark Theme + Environment Layout

Date: 2026-08-14  
Branch: `feat/music-ha-mcp`  
Status: Approved for planning

## Goal

Make the idle home screen readable on a black background and tidy the middle weather block, while keeping status chrome and the wake hint unchanged. Keep using the official XiaoZhi OTA/activation endpoint for company testing.

## Decisions

1. **Theme:** Default display theme is `dark` (black background, white/light text for the whole LCD UI, including chat). Existing light theme remains available via settings/MCP theme switch.
2. **Unchanged:** Top status bar layout/behavior; bottom wake-hint label text and placement (“说「小智」开始对话”).
3. **Environment block layout:**
   - Horizontal padding **20 px** on the home panel (left and right).
   - Environment text **left-aligned**.
   - Remove the separate leading weather-icon column that made the row feel cluttered.
   - **Line 1:** weather + temperature on one line, e.g. `晴 28°C`.
   - **Line 2+:** humidity and any future metrics (e.g. pressure) each on their own line.
4. **Colors:** Prefer white body text from the dark theme; weather / temperature / humidity may use distinct accent colors for hierarchy (e.g. cool / warm / green). Accents must remain readable on black.
5. **Clock block:** Remains centered above the environment block (time + date). Only the environment section changes alignment.
6. **Data scope this iteration:** Firmware still exposes weather / temp / humidity only. Pressure (and similar) is layout-ready as “one metric per line” when data exists later; no new protocol fields in this change unless already present.

## Non-goals

- Pointing `CONFIG_OTA_URL` at the home Raspberry Pi server (company network cannot reach it).
- Changing music MCP / LAN music URL.
- Redesigning status icons, chat bubbles beyond what the stock dark theme already provides.
- Adding pressure/HA sensor plumbing in this pass.

## Implementation sketch

| Area | Change |
|------|--------|
| Default theme | `LcdDisplay` settings load default `"dark"` instead of `"light"` (NVS may already store `"light"` on devices that have run before — either document a one-time NVS clear / theme set, or force default only when key missing; do **not** overwrite an explicit user-saved theme). |
| `SetupUI` home panel | `pad_hor` = 20; environment column left-aligned; combine weather+temp into one label (or two labels on one flex row without a separate icon column); humidity on the next line. |
| `SetHomeEnvironment` | Update API usage so line 1 is composed as `"{weather} {temp}"` (or equivalent), humidity alone on line 2. Keep Display virtual API compatible or minimally extend without breaking OLED no-ops. |
| `SetTheme` | Continue rebinding home label fonts/colors after assets apply (existing UAF fix). Apply accent colors for env lines if used. |
| Top / wake hint | No layout or copy changes. |

## Testing

1. Build + flash `my-voice-board`.
2. Cold boot → Wi-Fi → theme refresh → **no crash** (regression for font UAF).
3. Idle home: black background; clock centered; env left-aligned with 20 px side inset; `晴 28°C` then humidity on next line.
4. Status bar and wake hint visually unchanged in position/role.
5. Enter conversation: dark chat chrome acceptable; switch theme to light via existing path still works if exercised.

## Open note for implementer

Devices that already saved `theme=light` in NVS will keep light until the user switches or NVS is cleared. Spec intent is **default for new / unset** = dark, not forcibly rewriting stored preference unless product later asks for a migration.
