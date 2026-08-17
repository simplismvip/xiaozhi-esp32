"""Weather: Home Assistant first, Open-Meteo fallback."""

from __future__ import annotations

import time
from typing import Any, Optional

import httpx

# WMO weather interpretation codes → short Chinese labels
_WMO_ZH: dict[int, str] = {
    0: "晴",
    1: "晴",
    2: "多云",
    3: "阴",
    45: "雾",
    48: "雾",
    51: "小雨",
    53: "中雨",
    55: "大雨",
    56: "冻雨",
    57: "冻雨",
    61: "小雨",
    63: "中雨",
    65: "大雨",
    66: "冻雨",
    67: "冻雨",
    71: "小雪",
    73: "中雪",
    75: "大雪",
    77: "雪",
    80: "阵雨",
    81: "阵雨",
    82: "暴雨",
    85: "阵雪",
    86: "阵雪",
    95: "雷阵雨",
    96: "雷阵雨",
    99: "雷阵雨",
}

# Home Assistant weather condition → short Chinese
_HA_CONDITION_ZH: dict[str, str] = {
    "clear-night": "晴",
    "cloudy": "多云",
    "fog": "雾",
    "hail": "冰雹",
    "lightning": "雷",
    "lightning-rainy": "雷阵雨",
    "partlycloudy": "多云",
    "pouring": "暴雨",
    "rainy": "雨",
    "snowy": "雪",
    "snowy-rainy": "雨夹雪",
    "sunny": "晴",
    "windy": "大风",
    "windy-variant": "大风",
    "exceptional": "异常",
}

# Skip flaky geocoding for common cities (name → lat, lon)
_CITY_COORDS: dict[str, tuple[float, float]] = {
    "北京": (39.9042, 116.4074),
    "上海": (31.2304, 121.4737),
    "广州": (23.1291, 113.2644),
    "深圳": (22.5431, 114.0579),
    "杭州": (30.2741, 120.1551),
    "成都": (30.5728, 104.0668),
    "重庆": (29.4316, 106.9123),
    "武汉": (30.5928, 114.3055),
    "西安": (34.3416, 108.9398),
    "南京": (32.0603, 118.7969),
    "天津": (39.3434, 117.3616),
    "苏州": (31.2989, 120.5853),
}


def _speak_line(
    *,
    ok: bool,
    city: str,
    weather: str,
    temp: str,
    humidity: Optional[int],
) -> str:
    """One short sentence for TTS / voice reply."""
    if not ok:
        return "暂时查不到天气，请稍后再试。"
    temp_speak = (temp or "").replace("°C", "度").replace("℃", "度")
    parts: list[str] = [f"{city}现在{weather}"]
    if temp_speak and not temp_speak.startswith("--"):
        parts.append(f"气温{temp_speak}")
    if humidity is not None:
        parts.append(f"湿度{humidity}%")
    return "，".join(parts) + "。"


def _payload(
    *,
    ok: bool,
    city: str,
    weather: str,
    temp: str,
    humidity: Optional[int],
    source: str,
    error: Optional[str] = None,
) -> dict[str, Any]:
    humidity_text = f"湿度 {humidity}%" if humidity is not None else "湿度 --%"
    out: dict[str, Any] = {
        "ok": ok,
        "city": city,
        "weather": weather,
        "temp": temp,
        "humidity": humidity,
        "humidity_text": humidity_text,
        "source": source,
        "speak": _speak_line(
            ok=ok, city=city, weather=weather, temp=temp, humidity=humidity
        ),
        "wea_comment": weather,
        "cityname": city,
        "wea_now": temp,
    }
    if error:
        out["error"] = error
    return out


class WeatherService:
    def __init__(
        self,
        user_agent: str,
        cache_ttl_sec: int = 600,
        *,
        ha_base_url: str = "",
        ha_token: str = "",
        ha_weather_entity: str = "",
        ha_city_label: str = "家里",
        default_city: str = "北京",
    ) -> None:
        self._ua = user_agent
        self._ttl = cache_ttl_sec
        self._ha_base = (ha_base_url or "").rstrip("/")
        self._ha_token = ha_token or ""
        self._ha_entity = ha_weather_entity or ""
        self._ha_city = ha_city_label or "家里"
        self._default_city = default_city or "北京"
        self._cache: dict[str, tuple[float, dict[str, Any]]] = {}

    def _use_ha_for(self, city: str) -> bool:
        if not (self._ha_base and self._ha_token and self._ha_entity):
            return False
        # Only home aliases use HA; other cities go straight to Open-Meteo.
        aliases = {
            self._ha_city.casefold(),
            self._default_city.casefold(),
            "home",
            "家里",
            "我的家",
        }
        return city.casefold() in aliases

    def current_weather(self, city: str) -> dict[str, Any]:
        city = (city or "").strip() or self._default_city
        key = city.casefold()
        now = time.time()
        hit = self._cache.get(key)
        if hit and now - hit[0] < self._ttl and hit[1].get("ok"):
            return dict(hit[1])

        errors: list[str] = []

        if self._use_ha_for(city):
            try:
                payload = self._fetch_ha()
                self._cache[key] = (now, payload)
                return dict(payload)
            except Exception as exc:  # noqa: BLE001
                errors.append(f"ha:{exc}")

        try:
            payload = self._fetch_open_meteo(city)
            self._cache[key] = (now, payload)
            return dict(payload)
        except Exception as exc:  # noqa: BLE001
            errors.append(f"open-meteo:{exc}")
            return _payload(
                ok=False,
                city=city,
                weather="--",
                temp="--°C",
                humidity=None,
                source="open-meteo",
                error="; ".join(errors)[:200],
            )

    def _fetch_ha(self) -> dict[str, Any]:
        headers = {
            "Authorization": f"Bearer {self._ha_token}",
            "Content-Type": "application/json",
        }
        timeout = httpx.Timeout(8.0, connect=4.0)
        url = f"{self._ha_base}/api/states/{self._ha_entity}"
        with httpx.Client(timeout=timeout, headers=headers) as client:
            resp = client.get(url)
            resp.raise_for_status()
            data = resp.json()

        state = str(data.get("state") or "").strip().lower()
        if state in {"unavailable", "unknown", ""}:
            raise ValueError(f"ha weather unavailable: {state or 'empty'}")

        attrs = data.get("attributes") or {}
        weather = _HA_CONDITION_ZH.get(state, state or "未知")
        temp_c = attrs.get("temperature")
        humidity = attrs.get("humidity")

        temp_text = f"{int(round(float(temp_c)))}°C" if temp_c is not None else "--°C"
        humidity_i: Optional[int] = int(humidity) if humidity is not None else None

        return _payload(
            ok=True,
            city=self._ha_city,
            weather=weather,
            temp=temp_text,
            humidity=humidity_i,
            source="home-assistant",
        )

    def _fetch_open_meteo(self, city: str) -> dict[str, Any]:
        headers = {"User-Agent": self._ua, "Accept": "application/json"}
        timeout = httpx.Timeout(20.0, connect=10.0)
        with httpx.Client(timeout=timeout, headers=headers) as client:
            coords = _CITY_COORDS.get(city)
            if coords:
                lat, lon = coords
                resolved = city
            else:
                geo = client.get(
                    "https://geocoding-api.open-meteo.com/v1/search",
                    params={"name": city, "count": 1, "language": "zh"},
                )
                geo.raise_for_status()
                results = geo.json().get("results") or []
                if not results:
                    raise ValueError(f"city not found: {city}")

                place = results[0]
                lat = place["latitude"]
                lon = place["longitude"]
                resolved = place.get("name") or city

            wx = client.get(
                "https://api.open-meteo.com/v1/forecast",
                params={
                    "latitude": lat,
                    "longitude": lon,
                    "current": "temperature_2m,relative_humidity_2m,weather_code",
                    "timezone": "auto",
                },
            )
            wx.raise_for_status()
            current = wx.json().get("current") or {}

        code = int(current.get("weather_code", -1))
        weather = _WMO_ZH.get(code, "未知")
        temp_c = current.get("temperature_2m")
        humidity = current.get("relative_humidity_2m")

        temp_text = f"{int(round(float(temp_c)))}°C" if temp_c is not None else "--°C"
        humidity_i: Optional[int] = int(humidity) if humidity is not None else None

        return _payload(
            ok=True,
            city=resolved,
            weather=weather,
            temp=temp_text,
            humidity=humidity_i,
            source="open-meteo",
        )
