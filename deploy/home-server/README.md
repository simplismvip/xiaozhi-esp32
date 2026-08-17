# xiaozhi-home-server

Home Raspberry Pi deployment helpers for XiaoZhi: weather API, Docker compose, and server patches.

## Layout

| Path | Role |
|------|------|
| `xiaozhi-apis/` | FastAPI weather/TH (`:3030`). Idle screen + voice `get_weather` both call this. |
| `patches/get_weather.py` | Mount over stock plugin: local API + IP geolocation default |
| `patches/openai.py` | Optional LLM provider patch |
| `docker-compose.yml` | `xiaozhi-esp32-server` with data/models/patches mounts |
| `systemd/xiaozhi-apis.service` | Boot-time uvicorn for weather API |
| `config.example.yaml` | Overlay snippet (no secrets) |

## Screen vs voice

- **Idle LCD**: ESP32 `CONFIG_HOME_ENV_URL` → `GET /api/weather` (default city / HA「家里」).
- **Voice**: patched `get_weather` → same API; no city → resolve via public IP;「家里」→ HA.

## Quick deploy notes

1. Copy `xiaozhi-apis` to `~/apps/xiaozhi-apis`, `cp config.example.json config.json`, fill HA token.
2. Install systemd unit from `systemd/`.
3. Mount `patches/get_weather.py` into the container (see compose volumes).
4. Merge `config.example.yaml` `plugins.get_weather` into `data/.config.yaml`.
