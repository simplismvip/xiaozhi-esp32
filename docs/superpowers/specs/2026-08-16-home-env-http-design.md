# Design: Idle Home Environment HTTP Fetch

Date: 2026-08-16  
Status: approved

## Problem

Idle home UI hardcodes `晴 / 0°C / 湿度 0%`. Pi already serves live weather/TH at `GET /api/weather` (`xiaozhi-apis` :3030).

## Approach

1. Board config sets `CONFIG_HOME_ENV_URL` (full URL, e.g. `http://192.168.1.9:3030/api/weather`).
2. On idle enter and every ~10 minutes while idle, fetch JSON in a short FreeRTOS task (never block the main loop).
3. Parse `weather`, `temp`, `humidity_text` (fallback if missing). Apply via `Display::SetHomeEnvironment` on the main task with `Application::Schedule`.
4. Keep last successful values; on failure leave UI unchanged (or `--` if never succeeded).
5. Empty `CONFIG_HOME_ENV_URL` disables fetch (other boards unchanged).
6. Keep `xiaozhi-apis` up with a systemd unit on the Pi.

## Non-goals

- No protocol/server push.
- No city query param in firmware (server default / HA “家里”).
- No music or MCP changes.
