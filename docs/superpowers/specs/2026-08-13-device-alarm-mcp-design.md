# Design: Device-side Alarm MCP (Phase 1)

Date: 2026-08-13  
Branch: `feat/device-alarm-mcp`  
Status: approved for planning  
Reference: [Jason76789/xiaozhi-alarm-mcp](https://github.com/Jason76789/xiaozhi-alarm-mcp), desktop note `闹钟.md`

## Problem

Users want natural-language timers and alarms (“十分钟后叫我”, “早上七点的闹钟”). Timing must remain reliable on-device; the cloud only interprets intent and calls tools.

## Goals (phase 1)

1. Set one-shot countdown (`delay` seconds) and one-shot wall-clock alarms (`hour` + `minute`).
2. Query and delete alarms via device MCP tools.
3. Persist alarms in NVS across reboot.
4. On fire: play a local prompt sound; keep the device from deep sleep while any active unfired alarm exists.
5. Port into the official fork without depending on the reference repo’s `SendTextToTts` API.

## Non-goals (phase 1)

- Recurring / daily alarms (`repeat` + `interval` beyond a single fire).
- Looping local music until wake word.
- Cloud TTS sentence on fire (phase 2).
- Home Assistant timers as the primary path.
- Server-side cron that wakes the device.

## Architecture

```text
User speech → xiaozhi-server LLM → device MCP tools
  self.alarm.set / self.alarm.del / self.alarm.queryall
                    ↓
            AlarmManager (esp_timer + NVS)
                    ↓ fire
     Application main loop polls IsRing()
                    ↓
           PlaySound(local OGG) + StopRing on wake/interrupt
```

Timing, storage, and fire detection run entirely on the ESP32. The server does not schedule the alarm; it only invokes MCP when the user speaks.

## Components

### AlarmManager (`main/alarm_manager.{h,cc}`)

- Max 10 alarms.
- Fields (phase 1): `id`, `active`, `time` (unix), `name`. Keep `repeat`/`interval` in the struct for forward compatibility but treat every alarm as one-shot (fire once, then deactivate).
- `set_alarm(delay, hour, minute, name)`:
  - If `hour`/`minute` valid → next occurrence of that local wall time (if already past today, use tomorrow).
  - Else → `now + delay` (delay must be > 0).
- `delete_alarm_by_id` / `delete_alarm_by_keyword` / `query_all_alarms_json`.
- `IsRing()` / `GetCurrentAlarmName()` / `StopRing()` / `get_active_alarm_count()`.
- NVS namespace `alarm_clock` via existing `Settings`.
- One `esp_timer` per active alarm (or equivalent), matching reference behavior closely enough for phase 1.

### MCP tools (`main/mcp_server.cc` common tools)

Register for all boards (same pattern as volume / device status):

| Tool | Purpose |
|------|---------|
| `self.alarm.set` | Create alarm; params: `delay`, `hour`, `minute`, `name` (optional fields as in reference; omit or ignore `repeat`/`interval` in phase 1 or accept and force one-shot) |
| `self.alarm.del` | Delete by `id` or keyword `name` |
| `self.alarm.queryall` | JSON list of active alarms |

Tool descriptions must steer the LLM: prefer `hour`/`minute` for clock times; use `delay` for relative timers; do not use HA tools for alarms.

### Application integration (`main/application.cc`)

- Init `AlarmManager` during startup / MCP tool registration path (once).
- In the main event loop: if `IsRing()`, ensure audible alert via `PlaySound` with an existing sound asset (e.g. `OGG_EXCLAMATION` / popup—pick one available in this fork).
- On wake word or user interrupt while ringing: `StopRing()`.
- `CanEnterSleepMode()`: return false if `get_active_alarm_count() > 0`.

### Build

- Kconfig `CONFIG_USE_ALARM` (default y for development boards we care about, or always on if small).
- `main/CMakeLists.txt` include `alarm_manager.cc` when enabled.

## Error handling

- Full alarm slots → MCP returns failure JSON with a short Chinese message.
- Invalid params (no delay and no hour/minute) → failure.
- Overdue one-shot after reboot → mark inactive (do not instantly ring a missed one-shot unless we explicitly choose to; phase 1: skip overdue one-shots on load).
- Missing network at fire time → local sound still plays; no TTS required.

## Testing

1. Host/unit: none required beyond compile; optional JSON shape checks for query.
2. Device smoke:
   - “两分钟后叫我” → set succeeds; after ~2 min local sound.
   - “早上七点闹钟” (or near-future hour/minute) → listed in query; fires at wall time.
   - Query / delete by name or id.
   - Reboot with a future alarm → still listed and fires.
   - With an active alarm, device does not enter deep sleep path that would skip the timer.
3. Report what still needs physical hardware (all ringing tests).

## Phase 2 (out of scope now)

- `repeat` / `interval` daily alarms.
- Optional cloud TTS line after local sound (implement without inventing `SendTextToTts` if upstream has another speak path, or add a minimal one).
- Ring UI on idle home panel.

## Risks

- Reference fork APIs differ (P3 sounds, `SendTextToTts`, older state machine)—adapt, do not copy blindly.
- Sleep / power-save interaction must be verified on `my-voice-board`.
- LLM may still mis-compute `delay` for clock times—tool description + preferring `hour`/`minute` mitigates this (same lesson as the reference article).
