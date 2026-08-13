#!/usr/bin/env python3
"""Remote smoke checks for music_gener + HA MCP + device serial boot logs."""

from __future__ import annotations

import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

import serial


MCP_SETTINGS = "/home/ming/docker/xiaozhi-server/data/.mcp_server_settings.json"
RESOLVE_URL = "http://127.0.0.1:3020/api/device/play-resolve?song=%E9%9B%A8%E5%A4%A9"
SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_BAUD = 115200
SERIAL_SECONDS = 28


def http_get(url: str, timeout: float = 8.0, headers: dict | None = None) -> tuple[int, bytes]:
    req = urllib.request.Request(url, headers=headers or {})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read() if e.fp else b""


def quote_url(url: str) -> str:
    """Encode path/query so spaces and CJK are valid for urllib."""
    parts = urllib.parse.urlsplit(url)
    path = urllib.parse.quote(parts.path, safe="/")
    # Keep query separators; encode values loosely
    query = urllib.parse.quote(parts.query, safe="=&%")
    return urllib.parse.urlunsplit((parts.scheme, parts.netloc, path, query, parts.fragment))


def http_head(url: str, timeout: float = 8.0) -> tuple[int, list[str]]:
    req = urllib.request.Request(quote_url(url), method="HEAD")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            headers = [f"{k}: {v}" for k, v in resp.headers.items()]
            return resp.status, headers
    except urllib.error.HTTPError as e:
        headers = [f"{k}: {v}" for k, v in (e.headers.items() if e.headers else [])]
        return e.code, headers
    except urllib.error.URLError as e:
        return 0, [f"URLError: {e}"]


def section(title: str) -> None:
    print(f"\n=== {title} ===", flush=True)


def check_music() -> None:
    section("1) music_gener play-resolve + stream HEAD")
    code, body = http_get(RESOLVE_URL)
    print(f"resolve HTTP {code}")
    data = json.loads(body.decode("utf-8", "replace"))
    print(
        f"ok={data.get('ok')} title={data.get('title')!r} "
        f"artist={data.get('artist')!r} id={data.get('id')}"
    )
    url = data.get("stream_url") or ""
    print(f"stream_url={url[:140]}{'...' if len(url) > 140 else ''}")
    if not url:
        print("FAIL: no stream_url")
        return
    status, headers = http_head(url)
    print(f"stream HEAD HTTP {status}")
    for h in headers[:12]:
        print(h)


def check_ha() -> None:
    section("2) HA MCP SSE (Authorization from server settings)")
    with open(MCP_SETTINGS, encoding="utf-8") as f:
        cfg = json.load(f)
    ha = cfg["mcpServers"]["Home Assistant"]
    token = ha["env"]["API_ACCESS_TOKEN"]
    # Do not print token
    url = ha["args"][0]
    print(f"proxy target={url}")
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "text/event-stream",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            ctype = resp.headers.get("Content-Type", "")
            print(f"SSE HTTP {resp.status} content-type={ctype}")
            # Read a tiny bit then close; do not dump payloads
            chunk = resp.read(64)
            print(f"first_bytes={len(chunk)}")
    except urllib.error.HTTPError as e:
        print(f"SSE HTTP {e.code} (may still mean endpoint exists)")
    except Exception as e:
        print(f"SSE error: {type(e).__name__}: {e}")


def check_serial() -> None:
    section("3) serial boot filter (music/MCP)")
    keys = (
        "MCP",
        "music",
        "Music",
        "Esp32Music",
        "play_song",
        "self.music",
        "MyVoiceBoard",
        "ST7789",
        "Add tool",
    )
    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=0.2)
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(0.05)
        ser.reset_input_buffer()
        deadline = time.time() + SERIAL_SECONDS
        matched = 0
        lines = 0
        music_tools = 0
        while time.time() < deadline:
            raw = ser.readline()
            if not raw:
                continue
            lines += 1
            try:
                text = raw.decode("utf-8", "replace").rstrip()
            except Exception:
                continue
            if any(k.lower() in text.lower() for k in keys):
                matched += 1
                print(text)
                if "self.music" in text or "play_song" in text:
                    music_tools += 1
        print(f"--- captured_lines={lines} matched={matched} music_tool_hits={music_tools} ---")
        if music_tools == 0:
            print("WARN: no self.music tool lines in boot window")
        else:
            print("OK: music MCP tools seen in boot log")
    finally:
        ser.close()


def main() -> int:
    check_music()
    check_ha()
    check_serial()
    print("\nDone.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
