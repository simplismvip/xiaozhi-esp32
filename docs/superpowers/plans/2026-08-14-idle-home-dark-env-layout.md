# Idle Home Dark Theme + Env Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Default the LCD UI to dark theme and restructure idle-home weather/temp/humidity into a left-aligned, 20px-padded block with weather+temp on one line and humidity on the next, without changing the status bar or wake hint.

**Architecture:** Keep the existing `LvglTheme` light/dark pair. Change only the NVS default theme name to `"dark"`. Replace the idle-home `env_row` (icon column + stacked labels) with a left-aligned column: a horizontal weather+temp row, then humidity (future metrics = additional labels). Preserve the `SetTheme` home-font rebind that fixes the post-assets UAF crash.

**Tech Stack:** ESP-IDF, LVGL via `esp_lvgl_port`, `LcdDisplay` / `SpiLcdDisplay`, NVS `Settings("display")`.

**Spec:** `docs/superpowers/specs/2026-08-14-idle-home-dark-env-layout-design.md`

---

## File map

| File | Responsibility |
|------|----------------|
| `main/display/lcd_display.cc` | Default theme; `SetupUI` home env layout; `SetHomeEnvironment`; `SetTheme` accent/font rebind; keep UAF rebind |
| `main/display/lcd_display.h` | Drop or repurpose `home_weather_icon_` if removed; keep weather/temp/humidity label pointers |
| `main/application.cc` | Placeholder `SetHomeEnvironment` strings unchanged in meaning (`"--"`, `"--°C"`, `"湿度 --%"`) |
| `main/boards/my-voice-board/README.md` | Note dark default + env line layout |
| (already dirty) `main/display/lcd_display.cc` UAF rebind | Commit as part of Task 1 before layout edits, or same commit if implementing immediately after |

No new files. No OTA URL / music URL changes.

Accent colors (hex, on black):

| Role | Color |
|------|-------|
| Weather text | `0x7DD3FC` |
| Temperature | `0xFBBF24` |
| Humidity | `0x86EFAC` |
| Clock / date / wake hint | theme `text_color()` (white in dark) |

---

### Task 1: Commit pending font UAF fix (if still uncommitted)

**Files:**
- Modify: `main/display/lcd_display.cc` (already has SetTheme home font rebind + safer weather glyph)

- [ ] **Step 1: Confirm diff is only the UAF/safety fix**

Run:

```bash
cd /Users/ming/Desktop/xiaozhi-esp32-official
git diff main/display/lcd_display.cc
```

Expected: `SetTheme` home label rebind block; weather icon uses `text_font` and `"晴"` instead of `"☀"` / large icon font.

- [ ] **Step 2: Commit**

```bash
git add main/display/lcd_display.cc
git commit -m "$(cat <<'EOF'
fix(display): rebind idle home fonts after theme replace

Assets::Apply freed the previous LvglBuiltInFont while home labels
still held raw lv_font_t pointers, crashing LVGL after Wi-Fi.
EOF
)"
```

---

### Task 2: Default theme to dark (unset NVS only)

**Files:**
- Modify: `main/display/lcd_display.cc` (~line 82)

- [ ] **Step 1: Change default theme name**

In `LcdDisplay::LcdDisplay`, replace:

```cpp
std::string theme_name = settings.GetString("theme", "light");
```

with:

```cpp
std::string theme_name = settings.GetString("theme", "dark");
```

Do **not** call `settings.SetString` to overwrite an existing NVS value.

- [ ] **Step 2: Commit**

```bash
git add main/display/lcd_display.cc
git commit -m "$(cat <<'EOF'
feat(display): default LCD theme to dark when unset
EOF
)"
```

**Note for device under test:** If this board previously saved `theme=light`, idle home stays light until NVS is cleared or theme is switched. For company flash verification, either erase NVS (`idf.py erase-flash` then full flash) or set theme to dark once via MCP `self.screen.set_theme` / settings. Document in the flash step of Task 5.

---

### Task 3: Restructure idle-home environment layout

**Files:**
- Modify: `main/display/lcd_display.cc` (`SetupUI` home panel ~959–1046; destructor/cleanup ~322–331; `SetTheme` home block)
- Modify: `main/display/lcd_display.h` (remove `home_weather_icon_` if unused)

- [ ] **Step 1: Update home panel horizontal padding to 20**

In `SetupUI` where `home_panel_` is created, replace horizontal pad that uses `lvgl_theme->spacing(4)` with literal 20:

```cpp
lv_obj_set_style_pad_hor(home_panel_, 20, 0);
```

Keep `pad_top` / `pad_bottom` as they are (status bar clearance + bottom spacing). Keep flex `SPACE_BETWEEN` so clock stays top-ish, wake hint at bottom — **do not move** `home_wake_hint_label_`.

- [ ] **Step 2: Replace env_row + icon with left-aligned env_col**

Delete the current weather `env_row` that creates `home_weather_icon_` and a nested `env_col`. Replace with:

```cpp
    // --- environment block (left-aligned; weather+temp one line, humidity next) ---
    lv_obj_t* env_col = lv_obj_create(home_panel_);
    lv_obj_set_width(env_col, LV_PCT(100));
    lv_obj_set_height(env_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(env_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(env_col, 0, 0);
    lv_obj_set_style_pad_all(env_col, 0, 0);
    lv_obj_set_flex_flow(env_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(env_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(env_col, lvgl_theme->spacing(2), 0);
    lv_obj_set_scrollbar_mode(env_col, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* weather_temp_row = lv_obj_create(env_col);
    lv_obj_set_width(weather_temp_row, LV_PCT(100));
    lv_obj_set_height(weather_temp_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(weather_temp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_temp_row, 0, 0);
    lv_obj_set_style_pad_all(weather_temp_row, 0, 0);
    lv_obj_set_flex_flow(weather_temp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_temp_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(weather_temp_row, lvgl_theme->spacing(2), 0);
    lv_obj_set_scrollbar_mode(weather_temp_row, LV_SCROLLBAR_MODE_OFF);

    home_weather_label_ = lv_label_create(weather_temp_row);
    lv_obj_set_style_text_font(home_weather_label_, text_font, 0);
    lv_obj_set_style_text_color(home_weather_label_, lv_color_hex(0x7DD3FC), 0);
    lv_label_set_text(home_weather_label_, "--");

    home_temp_label_ = lv_label_create(weather_temp_row);
    lv_obj_set_style_text_font(home_temp_label_, text_font, 0);
    lv_obj_set_style_text_color(home_temp_label_, lv_color_hex(0xFBBF24), 0);
    lv_label_set_text(home_temp_label_, "--°C");

    home_humidity_label_ = lv_label_create(env_col);
    lv_obj_set_style_text_font(home_humidity_label_, text_font, 0);
    lv_obj_set_style_text_color(home_humidity_label_, lv_color_hex(0x86EFAC), 0);
    lv_label_set_text(home_humidity_label_, "湿度 --%");

    // Intentionally no home_weather_icon_; future metrics (e.g. pressure) = new labels under humidity.
```

- [ ] **Step 3: Clear icon pointer in header and teardown**

In `main/display/lcd_display.h`, remove:

```cpp
    lv_obj_t* home_weather_icon_ = nullptr;
```

In the `home_panel_` delete path in `lcd_display.cc`, remove `home_weather_icon_ = nullptr;`.

In `SetTheme`, remove the `home_weather_icon_` rebind block. Keep rebind for date/weather/temp/humidity/wake/time. Update weather/temp/humidity **colors** to accents (not only `text_color()`):

```cpp
    if (home_weather_label_ != nullptr) {
        lv_obj_set_style_text_font(home_weather_label_, text_font, 0);
        lv_obj_set_style_text_color(home_weather_label_, lv_color_hex(0x7DD3FC), 0);
    }
    if (home_temp_label_ != nullptr) {
        lv_obj_set_style_text_font(home_temp_label_, text_font, 0);
        lv_obj_set_style_text_color(home_temp_label_, lv_color_hex(0xFBBF24), 0);
    }
    if (home_humidity_label_ != nullptr) {
        lv_obj_set_style_text_font(home_humidity_label_, text_font, 0);
        lv_obj_set_style_text_color(home_humidity_label_, lv_color_hex(0x86EFAC), 0);
    }
```

Wake hint and clock continue to use `lvgl_theme->text_color()`.

- [ ] **Step 4: Keep `SetHomeEnvironment` API**

Leave signature as three `const char*`. Implementation still sets the three labels independently (callers pass weather, temp, humidity separately; UI places weather+temp on one visual row).

```cpp
void LcdDisplay::SetHomeEnvironment(const char* weather_text, const char* temp_text,
                                    const char* humidity_text) {
    DisplayLockGuard lock(this);
    if (home_weather_label_ != nullptr && weather_text != nullptr) {
        lv_label_set_text(home_weather_label_, weather_text);
    }
    if (home_temp_label_ != nullptr && temp_text != nullptr) {
        lv_label_set_text(home_temp_label_, temp_text);
    }
    if (home_humidity_label_ != nullptr && humidity_text != nullptr) {
        lv_label_set_text(home_humidity_label_, humidity_text);
    }
}
```

`Application::SetHomeMode` placeholder call stays:

```cpp
display->SetHomeEnvironment("--", "--°C", "湿度 --%");
```

- [ ] **Step 5: Format touched C++ files**

```bash
clang-format -i main/display/lcd_display.cc main/display/lcd_display.h
```

- [ ] **Step 6: Commit**

```bash
git add main/display/lcd_display.cc main/display/lcd_display.h
git commit -m "$(cat <<'EOF'
feat(display): left-align idle weather/temp/humidity layout

Weather and temperature share one row; humidity follows on the next
line with 20px side padding and accent colors on dark theme.
EOF
)"
```

---

### Task 4: Board README note

**Files:**
- Modify: `main/boards/my-voice-board/README.md`

- [ ] **Step 1: Add a short idle-home appearance note**

Near the existing idle-home section, add:

```markdown
Idle home appearance (2026-08-14):
- Default theme is `dark` when NVS has no saved theme.
- Environment block: left-aligned, 20px side inset; line 1 weather + temp; line 2 humidity (future metrics one per line).
- Status bar and wake hint unchanged.
```

- [ ] **Step 2: Commit**

```bash
git add main/boards/my-voice-board/README.md
git commit -m "$(cat <<'EOF'
docs(board): note dark idle home env layout
EOF
)"
```

---

### Task 5: Build, flash, hardware verify

**Files:** none (validation only)

- [ ] **Step 1: Build**

```bash
source /Users/ming/esp/esp-idf-v5.5.2/export.sh
cd /Users/ming/Desktop/xiaozhi-esp32-official
python3 scripts/build.py my-voice-board
```

Expected: build succeeds; `Project Version: 2.4.2` (or current).

- [ ] **Step 2: Flash (prefer full flash if NVS still has light theme)**

If previous boots saved light theme and screen is still white:

```bash
idf.py -p /dev/cu.usbserial-210 erase-flash
idf.py -p /dev/cu.usbserial-210 flash
```

Otherwise app flash is enough:

```bash
idf.py -p /dev/cu.usbserial-210 app-flash
```

- [ ] **Step 3: Serial smoke — no post-Wi-Fi crash**

Monitor ~60s after reset. Must see Wi-Fi IP, `Refreshing display theme`, `Ota: Current version`, and **no** `Guru Meditation` / `InstrFetchProhibited`.

- [ ] **Step 4: Visual check on device (idle)**

- Black background
- Clock + date still centered
- Middle: left-aligned `--` / `--°C` on one row, `湿度 --%` under it; ~20px from left/right edges
- Top status bar and bottom wake hint look as before
- Enter listening: home hides; chat uses dark chrome

Report what was verified on hardware vs build-only.

---

## Spec coverage checklist

| Spec item | Task |
|-----------|------|
| Default dark when unset | Task 2 |
| Do not overwrite saved theme | Task 2 |
| Status bar / wake hint unchanged | Task 3 (explicit non-touch) |
| pad_hor 20, left align | Task 3 |
| Weather + temp one line | Task 3 |
| Humidity (future metrics) own line | Task 3 |
| Accent colors | Task 3 |
| Keep SetTheme font rebind | Task 1 + Task 3 |
| No OTA/music changes | (out of plan) |
| README | Task 4 |
| Flash + visual verify | Task 5 |

## Self-review notes

- No TDD unit tests: LVGL home UI has no host harness in this repo; Task 5 is the verification gate.
- API stays three-string `SetHomeEnvironment`; composition is visual (two labels in one row), not a concatenated single string — matches callers and accent colors.
- Pressure not implemented; layout comment reserves “new label under humidity”.
