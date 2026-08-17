from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any, Optional

from fastapi import FastAPI, File, Form, HTTPException, Query, UploadFile
from pydantic import BaseModel

from .voice_clone import DEFAULT_MODEL, VoiceCloneError, VoiceCloneService
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
    minimax_api_key: str = ""
    minimax_api_base: str = "https://api.minimaxi.com"
    minimax_tts_model: str = DEFAULT_MODEL


class ClonePromptIn(BaseModel):
    prompt_audio: int
    prompt_text: str


class CloneBody(BaseModel):
    file_id: int
    voice_id: str
    clone_prompt: Optional[ClonePromptIn] = None
    text: Optional[str] = None
    model: Optional[str] = None
    language_boost: Optional[str] = None
    need_noise_reduction: bool = False
    need_volume_normalization: bool = False
    activate: bool = False
    activate_text: Optional[str] = None


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


@lru_cache(maxsize=1)
def get_voice_clone_service() -> VoiceCloneService:
    s = get_settings()
    return VoiceCloneService(
        api_key=s.minimax_api_key,
        api_base=s.minimax_api_base,
        default_model=s.minimax_tts_model,
    )


app = FastAPI(title="xiaozhi-apis", version="0.2.0")


def _http_from_voice_error(exc: VoiceCloneError) -> HTTPException:
    return HTTPException(
        status_code=exc.status_code,
        detail={"message": exc.message, "detail": exc.detail},
    )


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


@app.post("/api/voice/upload")
async def api_voice_upload(
    file: UploadFile = File(...),
    purpose: str = Form(default="voice_clone"),
) -> dict[str, Any]:
    content = await file.read()
    try:
        return await get_voice_clone_service().upload(
            purpose=purpose,
            filename=file.filename or "audio.bin",
            content=content,
        )
    except VoiceCloneError as exc:
        raise _http_from_voice_error(exc) from exc


@app.post("/api/voice/clone")
async def api_voice_clone(body: CloneBody) -> dict[str, Any]:
    svc = get_voice_clone_service()
    try:
        cloned = await svc.clone(
            file_id=body.file_id,
            voice_id=body.voice_id,
            clone_prompt=(
                body.clone_prompt.model_dump() if body.clone_prompt else None
            ),
            text=body.text,
            model=body.model,
            language_boost=body.language_boost,
            need_noise_reduction=body.need_noise_reduction,
            need_volume_normalization=body.need_volume_normalization,
        )
    except VoiceCloneError as exc:
        raise _http_from_voice_error(exc) from exc

    result: dict[str, Any] = {
        "voice_id": cloned["voice_id"],
        "preview_url": cloned.get("preview_url"),
        "activated": False,
        "activate_error": None,
        "extra_info": cloned.get("extra_info"),
    }
    if body.activate:
        try:
            act = await svc.activate(
                voice_id=cloned["voice_id"],
                text=body.activate_text,
                model=body.model,
            )
            result["activated"] = True
            result["activate"] = {"audio_hex_len": act.get("audio_hex_len")}
        except VoiceCloneError as exc:
            result["activated"] = False
            result["activate_error"] = {
                "message": exc.message,
                "detail": exc.detail,
            }
    return result


@app.post("/api/voice/clone-from-file")
async def api_voice_clone_from_file(
    file: UploadFile = File(...),
    voice_id: str = Form(...),
    prompt_file: Optional[UploadFile] = File(default=None),
    prompt_text: Optional[str] = Form(default=None),
    text: Optional[str] = Form(default=None),
    model: Optional[str] = Form(default=None),
    activate: bool = Form(default=False),
    activate_text: Optional[str] = Form(default=None),
) -> dict[str, Any]:
    content = await file.read()
    prompt_content = None
    prompt_filename = None
    if prompt_file is not None and prompt_file.filename:
        prompt_content = await prompt_file.read()
        prompt_filename = prompt_file.filename
    try:
        return await get_voice_clone_service().clone_from_file(
            voice_id=voice_id,
            filename=file.filename or "clone.mp3",
            content=content,
            prompt_filename=prompt_filename,
            prompt_content=prompt_content,
            prompt_text=prompt_text,
            text=text,
            model=model,
            activate=activate,
            activate_text=activate_text,
        )
    except VoiceCloneError as exc:
        raise _http_from_voice_error(exc) from exc
