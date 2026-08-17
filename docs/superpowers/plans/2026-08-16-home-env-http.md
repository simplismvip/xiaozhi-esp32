# Home Environment HTTP Implementation Plan

> **For agentic workers:** Implement task-by-task. Steps use checkbox syntax.

**Goal:** Idle home shows live weather/temp/humidity from Pi `xiaozhi-apis`.

**Architecture:** Kconfig URL → background HTTP GET → parse JSON → `Schedule` → `SetHomeEnvironment`. Cache last good values.

**Tech Stack:** ESP-IDF Http via Board network, cJSON, FreeRTOS task, systemd on Pi.

---

### Task 1: Kconfig + board config

- [ ] Add `CONFIG_HOME_ENV_URL` in `main/Kconfig.projbuild`
- [ ] Append URL in `main/boards/my-voice-board/config.json`
- [ ] Note in board README

### Task 2: Fetch helper + Application wiring

- [ ] Add `main/home_environment.{h,cc}` and CMakeLists entry
- [ ] Wire `SetHomeMode` + idle clock tick refresh (~600s)
- [ ] Format touched C/C++ with clang-format

### Task 3: Pi service + flash

- [ ] Install systemd unit for `xiaozhi-apis` on :3030
- [ ] Pull/build/flash `my-voice-board` on Pi
- [ ] Verify API + serial/UI path
