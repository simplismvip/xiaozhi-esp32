# Portrait Idle Home UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On `my-voice-board` SPI LCD, show a portrait idle home (clock, date, weather/temp/humidity placeholders, wake hint) and switch to stock emoji UI only while chatting; optional 30s idle backlight dim to 20%.

**Architecture:** Extend `Display` with home APIs (no-ops by default). Implement widgets in the default-style `LcdDisplay::SetupUI` path. Drive visibility and clock text from `Application::HandleStateChangedEvent` + existing `MAIN_EVENT_CLOCK_TICK`. Dim via one-shot `esp_timer` calling `Backlight::SetBrightness(20, false)` / `RestoreBrightness()`. Switch board logical resolution to portrait 240×320.

**Tech Stack:** ESP-IDF, LVGL 9 (`LcdDisplay` / `SpiLcdDisplay`), `esp_timer`, `PwmBacklight`, Application state machine.

**Spec:** `docs/superpowers/specs/2026-08-13-idle-home-portrait-ui-design.md`  
**Mockup:** `docs/superpowers/specs/assets/2026-08-13-idle-home-portrait-mockup.png`  
**Branch:** `feat/music-ha-mcp`

---

## File map

| File | Responsibility |
|------|----------------|
| `main/boards/my-voice-board/config.h` | Portrait 240×320 + IPS invert/mirror for carrier |
| `main/boards/my-voice-board/config.json` | Document / sdkconfig notes if needed |
| `main/boards/my-voice-board/README.md` | Idle UI + portrait + placeholders |
| `main/CMakeLists.txt` | Optional: larger default text font for this board |
| `main/display/display.h` | Virtual home APIs with no-op defaults |
| `main/display/lcd_display.h` | Home widget members + overrides |
| `main/display/lcd_display.cc` | Build home panel; setters; hide emoji when home visible |
| `main/application.h` / `application.cc` | Show/hide home; clock strings; dim timer |

No weather HTTP client in this plan. No battery ADC. No WeChat/Emote style forks (only `#else` default `SetupUI`).

---

### Task 1: Portrait orientation for my-voice-board

**Files:**
- Modify: `main/boards/my-voice-board/config.h`
- Modify: `main/boards/my-voice-board/README.md` (orientation note)
- Modify: `main/CMakeLists.txt` (SPI branch fonts — keep 16 for UI body; clock uses declared 30pt in LcdDisplay)

- [ ] **Step 1: Make IPS (default) profile portrait 240×320**

In `config.h`, replace the `CONFIG_MY_VOICE_LCD_ST7789_240X320` block so logical size is portrait. Keep IPS color invert. Start with no XY swap (tune mirrors on hardware if upside-down/mirrored):

```cpp
#if CONFIG_MY_VOICE_LCD_ST7789_240X320
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    true
#else
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
#define DISPLAY_INVERT_COLOR    false
#endif
```

If the panel appears rotated after flash, adjust only `DISPLAY_SWAP_XY` / `DISPLAY_MIRROR_*` — do not change the idle UI layout code.

- [ ] **Step 2: README orientation**

In `main/boards/my-voice-board/README.md`, state SPI UI is **portrait 240×320**; idle home shows clock/weather placeholders; chat shows emoji.

- [ ] **Step 3: Commit**

```bash
git add main/boards/my-voice-board/config.h main/boards/my-voice-board/README.md
git commit -m "fix(board): use portrait 240x320 for my-voice-board SPI LCD"
```

---

### Task 2: Display home API stubs

**Files:**
- Modify: `main/display/display.h`

- [ ] **Step 1: Add virtual methods**

After `SetPowerSaveMode` in `Display`:

```cpp
    // Idle home panel (clock / weather / environment). No-ops without LVGL home UI.
    virtual void SetHomeVisible(bool visible) { (void)visible; }
    virtual void SetHomeEnvironment(const char* weather_text,
                                    const char* temp_text,
                                    const char* humidity_text) {
        (void)weather_text;
        (void)temp_text;
        (void)humidity_text;
    }
    virtual void SetHomeClock(const char* time_text, const char* date_text) {
        (void)time_text;
        (void)date_text;
    }
```

`NoDisplay` inherits no-ops. No changes required in `display.cc`.

- [ ] **Step 2: Commit**

```bash
git add main/display/display.h
git commit -m "feat(display): add idle home panel API stubs"
```

---

### Task 3: LcdDisplay home panel (default message style)

**Files:**
- Modify: `main/display/lcd_display.h`
- Modify: `main/display/lcd_display.cc`

Only implement in the `#else` branch of `SetupUI()` (default message style, ~line 822+). Do **not** add home panel to WeChat-style `SetupUI`.

- [ ] **Step 1: Members + overrides in `lcd_display.h`**

Protected members:

```cpp
    lv_obj_t* home_panel_ = nullptr;
    lv_obj_t* home_time_label_ = nullptr;
    lv_obj_t* home_date_label_ = nullptr;
    lv_obj_t* home_weather_icon_ = nullptr;
    lv_obj_t* home_weather_label_ = nullptr;
    lv_obj_t* home_temp_label_ = nullptr;
    lv_obj_t* home_humidity_label_ = nullptr;
    lv_obj_t* home_wake_hint_label_ = nullptr;
    bool home_visible_ = false;
```

Public overrides:

```cpp
    virtual void SetHomeVisible(bool visible) override;
    virtual void SetHomeEnvironment(const char* weather_text,
                                    const char* temp_text,
                                    const char* humidity_text) override;
    virtual void SetHomeClock(const char* time_text, const char* date_text) override;
```

- [ ] **Step 2: Declare a larger clock font at top of `lcd_display.cc`**

Next to existing `LV_FONT_DECLARE` lines:

```cpp
LV_FONT_DECLARE(font_noto_sans_basic_30_4);
```

If link fails, fall back to `large_icon_font` / `text_font` for time (still implement layout).

- [ ] **Step 3: Create `home_panel_` in default `SetupUI()`**

After `top_bar_` / `status_bar_` creation (and before bottom chat bar setup), create a full-screen content column under the top bar:

```cpp
    home_panel_ = lv_obj_create(screen);
    lv_obj_set_size(home_panel_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_opa(home_panel_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(home_panel_, 0, 0);
    lv_obj_set_style_pad_top(home_panel_, 36, 0);  // clear status bar
    lv_obj_set_style_pad_bottom(home_panel_, lvgl_theme->spacing(4), 0);
    lv_obj_set_style_pad_hor(home_panel_, lvgl_theme->spacing(4), 0);
    lv_obj_set_flex_flow(home_panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(home_panel_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(home_panel_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(home_panel_, LV_OBJ_FLAG_HIDDEN);  // shown when idle

    // --- clock block ---
    lv_obj_t* clock_col = lv_obj_create(home_panel_);
    lv_obj_set_width(clock_col, LV_PCT(100));
    lv_obj_set_height(clock_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(clock_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_col, 0, 0);
    lv_obj_set_style_pad_all(clock_col, 0, 0);
    lv_obj_set_flex_flow(clock_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(clock_col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    home_time_label_ = lv_label_create(clock_col);
    lv_obj_set_style_text_font(home_time_label_, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(home_time_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(home_time_label_, "--:--");

    home_date_label_ = lv_label_create(clock_col);
    lv_obj_set_style_text_font(home_date_label_, text_font, 0);
    lv_obj_set_style_text_color(home_date_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(home_date_label_, "----");

    lv_obj_t* rule = lv_obj_create(clock_col);
    lv_obj_set_size(rule, 24, 2);
    lv_obj_set_style_radius(rule, 1, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rule, lvgl_theme->text_color(), 0);  // theme accent if available
    lv_obj_set_style_border_width(rule, 0, 0);

    // --- weather row ---
    lv_obj_t* env_row = lv_obj_create(home_panel_);
    lv_obj_set_width(env_row, LV_PCT(100));
    lv_obj_set_height(env_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(env_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(env_row, 0, 0);
    lv_obj_set_flex_flow(env_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(env_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(env_row, lvgl_theme->spacing(4), 0);

    home_weather_icon_ = lv_label_create(env_row);
    lv_obj_set_style_text_font(home_weather_icon_, large_icon_font, 0);
    lv_obj_set_style_text_color(home_weather_icon_, lvgl_theme->text_color(), 0);
    lv_label_set_text(home_weather_icon_, MATERIAL_SYMBOLS_WB_SUNNY);  // if symbol missing, use "☀" or ROBOT placeholder

    lv_obj_t* env_col = lv_obj_create(env_row);
    lv_obj_set_size(env_col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(env_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(env_col, 0, 0);
    lv_obj_set_flex_flow(env_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(env_col, 0, 0);

    home_weather_label_ = lv_label_create(env_col);
    lv_label_set_text(home_weather_label_, "--");
    home_temp_label_ = lv_label_create(env_col);
    lv_label_set_text(home_temp_label_, "--°C");
    home_humidity_label_ = lv_label_create(env_col);
    lv_label_set_text(home_humidity_label_, "湿度 --%");
    for (lv_obj_t* lab : {home_weather_label_, home_temp_label_, home_humidity_label_}) {
        lv_obj_set_style_text_font(lab, text_font, 0);
        lv_obj_set_style_text_color(lab, lvgl_theme->text_color(), 0);
    }

    home_wake_hint_label_ = lv_label_create(home_panel_);
    lv_obj_set_style_text_font(home_wake_hint_label_, text_font, 0);
    lv_obj_set_style_text_color(home_wake_hint_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_opa(home_wake_hint_label_, LV_OPA_60, 0);
    lv_label_set_text(home_wake_hint_label_, "说“小智”开始对话");
```

If `MATERIAL_SYMBOLS_WB_SUNNY` is not defined in the project symbol header, grep `MATERIAL_SYMBOLS_` for a sun/weather-like icon or use a short UTF-8 sun in `text_font`.

Ensure `home_panel_` is created as a **sibling above** `emoji_box_` in z-order when visible (create after emoji, or `lv_obj_move_foreground(home_panel_)` when showing).

- [ ] **Step 4: Implement setters**

```cpp
void LcdDisplay::SetHomeVisible(bool visible) {
    DisplayLockGuard lock(this);
    home_visible_ = visible;
    if (home_panel_ == nullptr) {
        return;
    }
    if (visible) {
        lv_obj_clear_flag(home_panel_, LV_OBJ_FLAG_HIDDEN);
        if (emoji_box_) {
            lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
        if (bottom_bar_) {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(home_panel_, LV_OBJ_FLAG_HIDDEN);
        if (emoji_box_) {
            lv_obj_clear_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
        if (bottom_bar_) {
            lv_obj_clear_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LcdDisplay::SetHomeEnvironment(const char* weather_text,
                                    const char* temp_text,
                                    const char* humidity_text) {
    DisplayLockGuard lock(this);
    if (home_weather_label_ && weather_text) {
        lv_label_set_text(home_weather_label_, weather_text);
    }
    if (home_temp_label_ && temp_text) {
        lv_label_set_text(home_temp_label_, temp_text);
    }
    if (home_humidity_label_ && humidity_text) {
        lv_label_set_text(home_humidity_label_, humidity_text);
    }
}

void LcdDisplay::SetHomeClock(const char* time_text, const char* date_text) {
    DisplayLockGuard lock(this);
    if (home_time_label_ && time_text) {
        lv_label_set_text(home_time_label_, time_text);
    }
    if (home_date_label_ && date_text) {
        lv_label_set_text(home_date_label_, date_text);
    }
}
```

Also update destructor / cleanup if `LcdDisplay` deletes child trees — usually screen delete is enough; if explicit `lv_obj_del` list exists, add `home_panel_`.

- [ ] **Step 5: Commit**

```bash
git add main/display/lcd_display.h main/display/lcd_display.cc
git commit -m "feat(display): add portrait idle home panel widgets"
```

---

### Task 4: Application — visibility, clock, backlight dim

**Files:**
- Modify: `main/application.h`
- Modify: `main/application.cc`

- [ ] **Step 1: Members in `Application`**

Private:

```cpp
    esp_timer_handle_t idle_dim_timer_ = nullptr;
    void StartIdleDimTimer();
    void CancelIdleDimTimer();
    void OnIdleDimTimer();
    void UpdateHomeClock();
    void SetHomeMode(bool home_visible);
```

- [ ] **Step 2: Helper implementations**

```cpp
void Application::UpdateHomeClock() {
    time_t now = time(nullptr);
    struct tm timeinfo = {};
    localtime_r(&now, &timeinfo);
    char time_buf[16];
    char date_buf[48];
    if (timeinfo.tm_year > (2016 - 1900)) {
        strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
        // Example: 8月13日 周四 — adjust format for locale; zh_CN may need manual weekday map
        static const char* kWeek[] = {"日", "一", "二", "三", "四", "五", "六"};
        snprintf(date_buf, sizeof(date_buf), "%d月%d日 周%s",
                 timeinfo.tm_mon + 1, timeinfo.tm_mday, kWeek[timeinfo.tm_wday]);
    } else {
        snprintf(time_buf, sizeof(time_buf), "--:--");
        snprintf(date_buf, sizeof(date_buf), "----");
    }
    Board::GetInstance().GetDisplay()->SetHomeClock(time_buf, date_buf);
}

void Application::SetHomeMode(bool home_visible) {
    auto display = Board::GetInstance().GetDisplay();
    display->SetHomeVisible(home_visible);
    if (home_visible) {
        display->SetHomeEnvironment("--", "--°C", "湿度 --%", /* ignored */);
        // Fix: SetHomeEnvironment has 3 string args only
        display->SetHomeEnvironment("--", "--°C", "湿度 --%");
        UpdateHomeClock();
    }
}

void Application::StartIdleDimTimer() {
    CancelIdleDimTimer();
    esp_timer_create_args_t args = {
        .callback = [](void* arg) {
            auto* app = static_cast<Application*>(arg);
            app->Schedule([app]() { app->OnIdleDimTimer(); });
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "idle_dim",
        .skip_unhandled_events = true,
    };
    if (idle_dim_timer_ == nullptr) {
        ESP_ERROR_CHECK(esp_timer_create(&args, &idle_dim_timer_));
    }
    ESP_ERROR_CHECK(esp_timer_start_once(idle_dim_timer_, 30 * 1000 * 1000ULL));
}

void Application::CancelIdleDimTimer() {
    if (idle_dim_timer_ && esp_timer_is_active(idle_dim_timer_)) {
        esp_timer_stop(idle_dim_timer_);
    }
}

void Application::OnIdleDimTimer() {
    if (GetDeviceState() != kDeviceStateIdle) {
        return;
    }
    auto* bl = Board::GetInstance().GetBacklight();
    if (bl) {
        bl->SetBrightness(20, false);
    }
}
```

Include `<ctime>` / `cstdio` as needed. Fix `SetHomeEnvironment` call — **exactly three** `const char*` args (no fourth).

Create timer once in `Application` ctor or first use; delete in dtor if Application owns it.

- [ ] **Step 3: Wire `HandleStateChangedEvent`**

In `kDeviceStateIdle` case, after existing status/emotion lines:

```cpp
            SetHomeMode(true);
            StartIdleDimTimer();
```

In `kDeviceStateConnecting`, `kDeviceStateListening`, `kDeviceStateSpeaking` (and wifi config if it shows chat UI):

```cpp
            CancelIdleDimTimer();
            if (auto* bl = Board::GetInstance().GetBacklight()) {
                bl->RestoreBrightness();
            }
            SetHomeMode(false);
```

For idle entry, stock code still calls `SetEmotion("neutral")` — `SetHomeVisible(true)` must hide emoji box so emotion is not visible underneath.

- [ ] **Step 4: Refresh clock on `MAIN_EVENT_CLOCK_TICK`**

Inside the existing `CLOCK_TICK` handler, after `UpdateStatusBar()`:

```cpp
            if (GetDeviceState() == kDeviceStateIdle) {
                UpdateHomeClock();
            }
```

- [ ] **Step 5: Commit**

```bash
git add main/application.h main/application.cc
git commit -m "feat(app): drive idle home panel, clock, and backlight dim"
```

---

### Task 5: Docs + build/flash verification

**Files:**
- Modify: `main/boards/my-voice-board/README.md` (idle section if not complete)

- [ ] **Step 1: Document acceptance behavior**

Add short section:

- Idle: clock / date / `--` weather placeholders / wake hint  
- Chat: emoji + subtitles  
- 30s idle → backlight 20%; leave idle restores  
- Weather API: later via `SetHomeEnvironment`

- [ ] **Step 2: Build on Pi**

```bash
. ~/esp/esp-idf/export.sh
cd ~/esp_apps/xiaozhi-esp32
# sync branch / rsync sources first
python3 scripts/build.py my-voice-board --name my-voice-board
```

Expected: build completes; flash `/dev/ttyUSB0`.

- [ ] **Step 3: Hardware checklist**

1. Portrait orientation looks upright (else tweak mirrors only).  
2. Idle matches mockup structure with placeholders.  
3. Clock updates at least each minute (tick is ~1s).  
4. Wake → home hidden, emoji visible.  
5. Return idle → home back; dim after ~30s.

- [ ] **Step 4: Commit docs**

```bash
git add main/boards/my-voice-board/README.md
git commit -m "docs(board): describe idle home UI behavior"
```

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| Portrait 240×320 | Task 1 |
| Idle clock/date/weather/TH/wake hint | Task 3–4 |
| Placeholders / SetHomeEnvironment API | Task 2–4 |
| Chat shows emoji, hide home | Task 3–4 |
| No live weather fetch | (explicit non-goal) |
| Optional 30s dim 20% | Task 4 |
| my-voice-board README | Task 1, 5 |

## Placeholder scan

Plan avoids TBD steps; Material sun symbol has an explicit fallback path. Orientation mirror tuning is hardware verification, not a code placeholder.

## Type consistency

- `SetHomeVisible(bool)`
- `SetHomeEnvironment(const char*, const char*, const char*)`
- `SetHomeClock(const char*, const char*)`
