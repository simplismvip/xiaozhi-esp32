"""Override stock get_weather: local xiaozhi-apis; default city from client IP."""

from __future__ import annotations

from typing import TYPE_CHECKING, Optional

import httpx
from config.logger import setup_logging
from plugins_func.register import Action, ActionResponse, ToolType, register_function

if TYPE_CHECKING:
    from core.connection import ConnectionHandler

TAG = __name__
logger = setup_logging()

# Container → host bridge (same pattern as HA MCP)
DEFAULT_WEATHER_API = "http://172.17.0.1:3030/api/weather"

HOME_ALIASES = {"家里", "我的家", "home", "本地", "本地天气", "这边", "这里"}

GET_WEATHER_FUNCTION_DESC = {
    "type": "function",
    "function": {
        "name": "get_weather",
        "description": (
            "查询天气并直接播报一句中文结果。"
            "用户问今天/现在天气且未指明城市时：不要传 location（按设备公网 IP 定位）。"
            "用户明确说某城市时传入该城市名。"
            "用户说家里天气时传入「家里」（走家庭传感器/HA）。"
            "不要用 Home Assistant 工具查天气。"
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "location": {
                    "type": "string",
                    "description": "地点，如上海、杭州；省略则按 IP 定位；家里则查家庭天气",
                },
                "lang": {
                    "type": "string",
                    "description": "语言 code，默认 zh_CN（兼容旧参数，可忽略）",
                },
            },
            "required": [],
        },
    },
}


def _normalize_city_name(city: str) -> str:
    city = (city or "").strip()
    if city.endswith("市") and len(city) > 2:
        city = city[:-1]
    return city


def _city_from_ip(conn: "ConnectionHandler") -> Optional[str]:
    try:
        from core.utils.util import get_ip_info

        ip = getattr(conn, "client_ip", None) or ""
        info = get_ip_info(ip, logger) or {}
        city = _normalize_city_name(str(info.get("city") or ""))
        if city and city not in {"未知位置", "未知", "None"}:
            return city
    except Exception as exc:  # noqa: BLE001
        logger.bind(tag=TAG).warning(f"ip location failed: {exc}")
    return None


def _resolve_city(conn: "ConnectionHandler", location: Optional[str], weather_config: dict) -> str:
    raw = (location or "").strip()
    if raw:
        if raw in {"今天", "今日"}:
            raw = ""
        elif raw in HOME_ALIASES:
            return "家里"
        else:
            return _normalize_city_name(raw)

    # No explicit city: prefer IP; optional config fallback (avoid stock 广州).
    ip_city = _city_from_ip(conn)
    if ip_city:
        logger.bind(tag=TAG).info(f"weather city from IP: {ip_city}")
        return ip_city

    fallback = (weather_config.get("default_location") or "").strip()
    if fallback and fallback not in {"广州"}:
        return _normalize_city_name(fallback)
    return "家里"


@register_function("get_weather", GET_WEATHER_FUNCTION_DESC, ToolType.SYSTEM_CTL)
async def get_weather(
    conn: "ConnectionHandler",
    location: str = None,
    lang: str = "zh_CN",
):
    weather_config = conn.config.get("plugins", {}).get("get_weather", {}) or {}
    api_url = weather_config.get("api_url", DEFAULT_WEATHER_API)
    city = _resolve_city(conn, location, weather_config)

    try:
        async with httpx.AsyncClient(timeout=httpx.Timeout(12.0, connect=4.0)) as client:
            resp = await client.get(api_url, params={"city": city})
            resp.raise_for_status()
            data = resp.json()
    except Exception as exc:  # noqa: BLE001
        logger.bind(tag=TAG).error(f"weather api failed: {exc}")
        return ActionResponse(
            Action.RESPONSE, "暂时查不到天气，请稍后再试。", None
        )

    speak = (data or {}).get("speak")
    if not speak:
        city_name = (data or {}).get("city") or city
        weather = (data or {}).get("weather") or "--"
        temp = str((data or {}).get("temp") or "--").replace("°C", "度")
        humidity = (data or {}).get("humidity")
        if (data or {}).get("ok"):
            parts = [f"{city_name}现在{weather}", f"气温{temp}"]
            if humidity is not None:
                parts.append(f"湿度{humidity}%")
            speak = "，".join(parts) + "。"
        else:
            speak = "暂时查不到天气，请稍后再试。"

    return ActionResponse(Action.RESPONSE, speak, None)
