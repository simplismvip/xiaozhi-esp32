# xiaozhi-apis

小智设备侧辅助 HTTP 服务（树莓派 `~/apps/xiaozhi-apis`）。天气是第一个接口，后续共用此项目扩展。

## 运行

```bash
cd ~/apps/xiaozhi-apis
chmod +x run.sh
./run.sh
```

默认监听 `0.0.0.0:3030`。

## 接口

### `GET /health`

```json
{"status":"ok"}
```

### `GET /api/weather?city=可选`

- `city` 可省略，使用 `config.json` 的 `default_city`
- 默认城市 / `家里` / `我的家` / `home`：**先读 Home Assistant 天气实体**，失败再走 Open-Meteo
- 其它城市：直接 Open-Meteo

成功示例（HA）：

```json
{
  "ok": true,
  "city": "家里",
  "weather": "雨",
  "temp": "28°C",
  "humidity": 87,
  "humidity_text": "湿度 87%",
  "source": "home-assistant",
  "wea_comment": "雨",
  "cityname": "家里",
  "wea_now": "28°C"
}
```

失败时仍返回 HTTP 200，`ok: false`，`weather`/`temp` 为 `--` 占位。

响应里带 `speak` 字段（一句中文，给 TTS / 小智直接播报），例如：
`家里现在雨，气温28度，湿度87%。`

### MiniMax 音色复刻

代理 [音色快速复刻](https://platform.minimaxi.com/docs/guides/speech-voice-clone)。需在 `config.json` 配置 `minimax_api_key`。克隆得到的 `voice_id` 自行填入小智 server 的 TTS 配置；不自动改 `.config.yaml`。

未 `activate` 时，音色为临时态：需在 7 天内做一次真正的 T2A（或本接口 `activate=true`）才会永久保留。

#### `POST /api/voice/upload`（分步）

```bash
curl -F purpose=voice_clone -F file=@clone_input.mp3 \
  http://127.0.0.1:3030/api/voice/upload
```

`purpose`：`voice_clone` 或 `prompt_audio`。

#### `POST /api/voice/clone`（分步）

```bash
curl -X POST http://127.0.0.1:3030/api/voice/clone \
  -H 'Content-Type: application/json' \
  -d '{"file_id":123,"voice_id":"MyVoice01","activate":false}'
```

可选：`clone_prompt`、`text`、`model`、`activate`（默认 `false`）、`activate_text`。

#### `POST /api/voice/clone-from-file`（一站式）

```bash
curl -F voice_id=MyVoice01 -F file=@clone_input.mp3 \
  -F activate=false \
  http://127.0.0.1:3030/api/voice/clone-from-file
```

可选：`prompt_file` + `prompt_text`、`text`、`model`、`activate`、`activate_text`。

## 配置

见 `config.json`：

- `ha_base_url` / `ha_token` / `ha_weather_entity`：HA REST
- `ha_city_label`：HA 成功时返回的城市显示名
- `default_city`：无 `city` 参数时的默认值（会走 HA 优先逻辑）
- `cache_ttl_sec`：成功结果内存缓存（失败不缓存）
- `minimax_api_key` / `minimax_api_base` / `minimax_tts_model`：音色复刻与可选 T2A 激活
