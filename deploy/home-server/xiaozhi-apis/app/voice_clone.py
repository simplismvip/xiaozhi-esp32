"""MiniMax voice-clone proxy helpers."""

from __future__ import annotations

import re
from typing import Any, Optional

import httpx

ALLOWED_EXTS = {".mp3", ".m4a", ".wav"}
MAX_UPLOAD_BYTES = 20 * 1024 * 1024
VOICE_ID_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_-]{6,254}[A-Za-z0-9]$")
DEFAULT_MODEL = "speech-2.6-turbo"
DEFAULT_ACTIVATE_TEXT = "你好，我是小智。"
UPLOAD_PURPOSES = {"voice_clone", "prompt_audio"}


class VoiceCloneError(Exception):
    def __init__(self, message: str, *, status_code: int = 400, detail: Any = None):
        super().__init__(message)
        self.message = message
        self.status_code = status_code
        self.detail = detail


class VoiceCloneService:
    def __init__(
        self,
        *,
        api_key: str,
        api_base: str = "https://api.minimaxi.com",
        default_model: str = DEFAULT_MODEL,
        timeout_sec: float = 120.0,
    ) -> None:
        self.api_key = (api_key or "").strip()
        self.api_base = (api_base or "https://api.minimaxi.com").rstrip("/")
        self.default_model = default_model or DEFAULT_MODEL
        self.timeout_sec = timeout_sec

    def ensure_configured(self) -> None:
        if not self.api_key:
            raise VoiceCloneError(
                "minimax_api_key not configured",
                status_code=503,
            )

    @staticmethod
    def validate_voice_id(voice_id: str) -> str:
        vid = (voice_id or "").strip()
        if not VOICE_ID_RE.fullmatch(vid) or not (8 <= len(vid) <= 256):
            raise VoiceCloneError(
                "invalid voice_id: length 8-256, start with letter, "
                "letters/digits/-/_, must not end with - or _"
            )
        return vid

    @staticmethod
    def validate_purpose(purpose: str) -> str:
        p = (purpose or "").strip()
        if p not in UPLOAD_PURPOSES:
            raise VoiceCloneError(
                f"invalid purpose: expected one of {sorted(UPLOAD_PURPOSES)}"
            )
        return p

    @staticmethod
    def validate_upload(filename: str, size: int) -> str:
        name = (filename or "audio.bin").strip() or "audio.bin"
        lower = name.lower()
        ext = ""
        for candidate in ALLOWED_EXTS:
            if lower.endswith(candidate):
                ext = candidate
                break
        if not ext:
            raise VoiceCloneError("unsupported file type: use mp3, m4a, or wav")
        if size <= 0:
            raise VoiceCloneError("empty file")
        if size > MAX_UPLOAD_BYTES:
            raise VoiceCloneError("file too large: max 20MB")
        return name

    def _headers_json(self) -> dict[str, str]:
        return {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
        }

    def _headers_auth(self) -> dict[str, str]:
        return {"Authorization": f"Bearer {self.api_key}"}

    def _raise_from_base_resp(self, body: dict[str, Any], *, action: str) -> None:
        base = body.get("base_resp") or {}
        code = base.get("status_code")
        if code in (0, None) and "base_resp" not in body:
            return
        if code == 0:
            return
        raise VoiceCloneError(
            f"minimax {action} failed: {base.get('status_msg') or code}",
            status_code=502,
            detail={
                "status_code": code,
                "status_msg": base.get("status_msg"),
            },
        )

    async def upload(self, *, purpose: str, filename: str, content: bytes) -> dict[str, Any]:
        self.ensure_configured()
        purpose = self.validate_purpose(purpose)
        filename = self.validate_upload(filename, len(content))
        url = f"{self.api_base}/v1/files/upload"
        files = {"file": (filename, content)}
        data = {"purpose": purpose}
        async with httpx.AsyncClient(timeout=self.timeout_sec) as client:
            resp = await client.post(
                url, headers=self._headers_auth(), data=data, files=files
            )
        try:
            body = resp.json()
        except Exception as exc:  # noqa: BLE001
            raise VoiceCloneError(
                f"minimax upload bad response: HTTP {resp.status_code}",
                status_code=502,
                detail=resp.text[:500],
            ) from exc
        if resp.status_code >= 400:
            raise VoiceCloneError(
                f"minimax upload HTTP {resp.status_code}",
                status_code=502,
                detail=body,
            )
        self._raise_from_base_resp(body, action="upload")
        file_id = (body.get("file") or {}).get("file_id")
        if file_id is None:
            raise VoiceCloneError(
                "minimax upload missing file_id",
                status_code=502,
                detail=body,
            )
        return {"file_id": file_id, "purpose": purpose}

    async def clone(
        self,
        *,
        file_id: int,
        voice_id: str,
        clone_prompt: Optional[dict[str, Any]] = None,
        text: Optional[str] = None,
        model: Optional[str] = None,
        language_boost: Optional[str] = None,
        need_noise_reduction: bool = False,
        need_volume_normalization: bool = False,
    ) -> dict[str, Any]:
        self.ensure_configured()
        voice_id = self.validate_voice_id(voice_id)
        payload: dict[str, Any] = {
            "file_id": int(file_id),
            "voice_id": voice_id,
            "need_noise_reduction": need_noise_reduction,
            "need_volume_normalization": need_volume_normalization,
        }
        if clone_prompt:
            prompt_audio = clone_prompt.get("prompt_audio")
            prompt_text = (clone_prompt.get("prompt_text") or "").strip()
            if prompt_audio is None or not prompt_text:
                raise VoiceCloneError(
                    "clone_prompt requires both prompt_audio and prompt_text"
                )
            payload["clone_prompt"] = {
                "prompt_audio": int(prompt_audio),
                "prompt_text": prompt_text,
            }
        preview_text = (text or "").strip()
        use_model = (model or self.default_model).strip() or self.default_model
        if preview_text:
            if len(preview_text) > 1000:
                raise VoiceCloneError("text too long: max 1000 chars")
            payload["text"] = preview_text
            payload["model"] = use_model
        if language_boost:
            payload["language_boost"] = language_boost

        url = f"{self.api_base}/v1/voice_clone"
        async with httpx.AsyncClient(timeout=self.timeout_sec) as client:
            resp = await client.post(url, headers=self._headers_json(), json=payload)
        try:
            body = resp.json()
        except Exception as exc:  # noqa: BLE001
            raise VoiceCloneError(
                f"minimax clone bad response: HTTP {resp.status_code}",
                status_code=502,
                detail=resp.text[:500],
            ) from exc
        if resp.status_code >= 400:
            raise VoiceCloneError(
                f"minimax clone HTTP {resp.status_code}",
                status_code=502,
                detail=body,
            )
        self._raise_from_base_resp(body, action="clone")
        return {
            "voice_id": voice_id,
            "preview_url": body.get("demo_audio") or None,
            "extra_info": body.get("extra_info"),
            "raw_base_resp": body.get("base_resp"),
        }

    async def activate(
        self,
        *,
        voice_id: str,
        text: Optional[str] = None,
        model: Optional[str] = None,
    ) -> dict[str, Any]:
        """First real T2A use to keep a rapid-clone voice permanent (within 7 days)."""
        self.ensure_configured()
        voice_id = self.validate_voice_id(voice_id)
        use_text = (text or DEFAULT_ACTIVATE_TEXT).strip() or DEFAULT_ACTIVATE_TEXT
        use_model = (model or self.default_model).strip() or self.default_model
        payload = {
            "model": use_model,
            "text": use_text,
            "stream": False,
            "voice_setting": {
                "voice_id": voice_id,
                "speed": 1,
                "vol": 1,
                "pitch": 0,
            },
            "audio_setting": {
                "sample_rate": 24000,
                "bitrate": 128000,
                "format": "mp3",
                "channel": 1,
            },
        }
        url = f"{self.api_base}/v1/t2a_v2"
        async with httpx.AsyncClient(timeout=self.timeout_sec) as client:
            resp = await client.post(url, headers=self._headers_json(), json=payload)
        try:
            body = resp.json()
        except Exception as exc:  # noqa: BLE001
            raise VoiceCloneError(
                f"minimax activate bad response: HTTP {resp.status_code}",
                status_code=502,
                detail=resp.text[:500],
            ) from exc
        if resp.status_code >= 400:
            raise VoiceCloneError(
                f"minimax activate HTTP {resp.status_code}",
                status_code=502,
                detail=body,
            )
        self._raise_from_base_resp(body, action="activate")
        audio = ((body.get("data") or {}).get("audio")) or ""
        return {
            "ok": True,
            "audio_hex_len": len(audio),
            "raw_base_resp": body.get("base_resp"),
        }

    async def clone_from_file(
        self,
        *,
        voice_id: str,
        filename: str,
        content: bytes,
        prompt_filename: Optional[str] = None,
        prompt_content: Optional[bytes] = None,
        prompt_text: Optional[str] = None,
        text: Optional[str] = None,
        model: Optional[str] = None,
        activate: bool = False,
        activate_text: Optional[str] = None,
    ) -> dict[str, Any]:
        uploaded = await self.upload(
            purpose="voice_clone", filename=filename, content=content
        )
        clone_prompt = None
        prompt_file_id = None
        if prompt_content:
            if not (prompt_text or "").strip():
                raise VoiceCloneError("prompt_text required when prompt_file is provided")
            prompt_uploaded = await self.upload(
                purpose="prompt_audio",
                filename=prompt_filename or "prompt.mp3",
                content=prompt_content,
            )
            prompt_file_id = prompt_uploaded["file_id"]
            clone_prompt = {
                "prompt_audio": prompt_file_id,
                "prompt_text": prompt_text.strip(),
            }

        cloned = await self.clone(
            file_id=uploaded["file_id"],
            voice_id=voice_id,
            clone_prompt=clone_prompt,
            text=text,
            model=model,
        )
        result: dict[str, Any] = {
            "voice_id": cloned["voice_id"],
            "file_id": uploaded["file_id"],
            "prompt_file_id": prompt_file_id,
            "preview_url": cloned.get("preview_url"),
            "activated": False,
            "activate_error": None,
        }
        if activate:
            try:
                act = await self.activate(
                    voice_id=cloned["voice_id"],
                    text=activate_text,
                    model=model,
                )
                result["activated"] = True
                result["activate"] = {
                    "audio_hex_len": act.get("audio_hex_len"),
                }
            except VoiceCloneError as exc:
                result["activated"] = False
                result["activate_error"] = {
                    "message": exc.message,
                    "detail": exc.detail,
                }
        return result
