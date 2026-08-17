from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any, Optional

from fastapi import FastAPI, Query
from pydantic import BaseModel, Field

from .weather import WeatherService

ROOT = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT / "config.json"


class Settings(BaseModel):
    port: int = 3030
    host: str = "0.0.0.0"
    default_city: str = "北京"
    cache_ttl_sec: int = 600
    user_agent: str = "Mozilla/5.0 (compatible; xiaozhi-apis/0.1)"
    ha_base_url: str = ""
    ha_token: str = ""
    ha_weather_entity: str = ""
    ha_city_label: str = "家里"


@lru_cache(maxsize=1)
def get_settings() -> Settings:
    raw: dict[str, Any] = {}
    if CONFIG_PATH.exists():
        loaded = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        if isinstance(loaded, dict):
            raw = loaded
    return Settings(**{k: v for k, v in raw.items() if k in Settings.model_fields})


@lru_cache(maxsize=1)
def get_weather_service() -> WeatherService:
    s = get_settings()
    return WeatherService(
        user_agent=s.user_agent,
        cache_ttl_sec=s.cache_ttl_sec,
        ha_base_url=s.ha_base_url,
        ha_token=s.ha_token,
        ha_weather_entity=s.ha_weather_entity,
        ha_city_label=s.ha_city_label,
        default_city=s.default_city,
    )


app = FastAPI(title="xiaozhi-apis", version="0.1.0")


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/api/weather")
def api_weather(
    city: Optional[str] = Query(default=None, description="城市名，可省略；默认走 HA 家里天气"),
) -> dict[str, Any]:
    s = get_settings()
    target = (city or s.default_city).strip() or s.default_city
    return get_weather_service().current_weather(target)
