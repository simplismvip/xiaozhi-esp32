# Device Alarm MCP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add device-side one-shot timers/alarms via MCP tools (`self.alarm.*`), with NVS persistence and local ring sound.

**Architecture:** Port a simplified `AlarmManager` (esp_timer + NVS) from the reference fork; register three common MCP tools; wake the main loop via `Schedule` / poll on `CLOCK_TICK`; block deep sleep while future alarms exist. Phase 1 forces one-shot only (ignore recurring).

**Tech Stack:** ESP-IDF C++, existing `Settings`/NVS, `McpServer::AddTool`, `Application::PlaySound` / `Schedule`, `Lang::Sounds::OGG_EXCLAMATION`.

**Spec:** `docs/superpowers/specs/2026-08-13-device-alarm-mcp-design.md`

---

## File map

| File | Role |
|------|------|
| Create `main/alarm_manager.h` | Alarm struct + `AlarmManager` API |
| Create `main/alarm_manager.cc` | Timers, NVS, ring state |
| Modify `main/CMakeLists.txt` | Add `alarm_manager.cc` under `CONFIG_USE_ALARM` |
| Modify `main/Kconfig.projbuild` | `CONFIG_USE_ALARM` default y |
| Modify `main/mcp_server.cc` | Register `self.alarm.set/del/queryall` |
| Modify `main/application.cc` | Ring handling on clock tick / wake; sleep guard |
| Modify `main/boards/my-voice-board/README.md` | Short alarm usage note |

---

### Task 1: AlarmManager

**Files:**
- Create: `main/alarm_manager.h`
- Create: `main/alarm_manager.cc`

- [ ] **Step 1: Add header** with `MAX_ALARMS 10`, `Alarm` struct (`id`, `active`, `time`, `repeat`, `interval`, `name`), singleton `AlarmManager` with `init`, `set_alarm`, delete/query, `IsRing` / `StopRing` / `GetCurrentAlarmName` / `get_active_alarm_count`, private timer helpers.

- [ ] **Step 2: Implement** based on reference `alarm_manager.cc`, with phase-1 changes:
  - Clock mode only if `hour` in `[0,23]` **and** `minute` in `[0,59]`; else require `delay > 0`.
  - Always one-shot: force `repeat = 1`, `interval = 0`; on fire set `active = false` (no reschedule).
  - Overdue on load → deactivate (no instant ring).
  - On fire: set ring flags, `save_alarms()`, `Application::GetInstance().Schedule(...)` empty or no-op to poke main loop (do not call `WakeUp` — not in this fork).
  - Include `alarm_manager.h`, `settings.h`, `application.h`, `cJSON.h`, `esp_timer.h`.

- [ ] **Step 3: Commit**

```bash
git add main/alarm_manager.h main/alarm_manager.cc
git commit -m "feat(alarm): add AlarmManager with NVS and esp_timer"
```

---

### Task 2: Build / Kconfig

**Files:**
- Modify: `main/CMakeLists.txt` (SOURCES list near `application.cc`)
- Modify: `main/Kconfig.projbuild` (new bool near other feature toggles)

- [ ] **Step 1:** Add:

```kconfig
config USE_ALARM
    bool "Enable device-side alarm MCP"
    default y
    help
        Local timers/alarms via self.alarm.* MCP tools.
```

- [ ] **Step 2:** Conditionally append `alarm_manager.cc` when `CONFIG_USE_ALARM`.

- [ ] **Step 3: Commit**

```bash
git commit -am "build: gate alarm manager with CONFIG_USE_ALARM"
```

---

### Task 3: MCP tools

**Files:**
- Modify: `main/mcp_server.cc` (`AddCommonTools`, after speaker/volume tools)

- [ ] **Step 1:** `#if CONFIG_USE_ALARM` include `alarm_manager.h` and add tools matching reference names/params, but descriptions say phase-1 one-shot only; `set_alarm` still accepts `repeat`/`interval` for LLM compat but manager ignores recurring.
- [ ] **Step 2: Commit**

```bash
git commit -am "feat(mcp): register self.alarm set/del/queryall tools"
```

---

### Task 4: Application ring + sleep

**Files:**
- Modify: `main/application.cc`
- Modify: `main/application.h` only if a small helper is needed

- [ ] **Step 1:** In `MAIN_EVENT_CLOCK_TICK` (and/or after schedule), if `CONFIG_USE_ALARM` and `AlarmManager::get_instance()->IsRing()`:
  - Play `Lang::Sounds::OGG_EXCLAMATION` once per ring cycle (static flag until `StopRing`).
  - Optional: `Alert` with alarm name for on-screen hint.
- [ ] **Step 2:** In `HandleWakeWordDetectedEvent` (start) call `StopRing()` when ringing.
- [ ] **Step 3:** In `CanEnterSleepMode`, return false if `get_active_alarm_count() > 0`.
- [ ] **Step 4: Commit**

```bash
git commit -am "feat(app): ring local alarm sound and block sleep"
```

---

### Task 5: Board docs + push

**Files:**
- Modify: `main/boards/my-voice-board/README.md`

- [ ] **Step 1:** Document voice examples and MCP tool names.
- [ ] **Step 2: Commit + push** `feat/device-alarm-mcp`.
- [ ] **Step 3:** Note for user: build/flash on Pi still required for hardware validation.

---

## Manual test plan (device)

1. Build `my-voice-board` with `CONFIG_USE_ALARM=y`.
2. “一分钟后叫我” → tool success → ~60s later `OGG_EXCLAMATION`.
3. Set near-future `hour`/`minute` → `queryall` lists it → fires.
4. Delete by keyword → gone from query.
5. Set future alarm, reboot → still listed.
6. Confirm sleep path blocked while alarm pending (log / behavior).
