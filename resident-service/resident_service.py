#!/usr/bin/env python3
#
#  resident_service.py
#  resident-service
#
#  The resident service: programs drive AI residents' and bots' bodies in this world through it (RESIDENT-API.md).
#  It runs beside the world's domain server, listens on 127.0.0.1 only, checks each program's resident key with the
#  metaverse, starts one assignment-client per body and stops it by its recorded pid, and enforces the rules (limits,
#  who may come in) before anything reaches a body. Standard library plus `websockets` (requirements.txt).
#
#      python3 resident_service.py [--config config.json]
#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import math
import os
import re
import secrets
import signal
import subprocess
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

from websockets.asyncio.server import serve
from websockets.datastructures import Headers
from websockets.exceptions import ConnectionClosed
from websockets.http11 import Response

HERE = Path(__file__).resolve().parent
API_VERSION = 1
log = logging.getLogger("resident-service")

DEFAULTS = {
    "port": 40105,                                   # the world's block base + 5, on 127.0.0.1 only
    "metaverse": "https://vr.darkride.ai",
    "world": {"address": "127.0.0.1", "udp_port": 40102},
    "residents": [],                                 # usernames the world's owner lets in
    "assignment_client": "",                         # "" = the assignment-client beside this folder
    "body_script": "../scripts/ai-residents/resident-body.js",
    "animations": "animations",                     # the bodies' clips (idle, walk, talk, gestures), served here
    "avatars": {},                                   # resident -> its avatar's public model URL ("" = the default)
    "logs": "logs",
    "limits": {"say_per_minute": 20, "requests_per_second": 20, "speed_max": 6.0},
    "body_grace_seconds": 60,                        # a body stays this long after its program goes away
    "body_start_seconds": 60,                        # how long a body may take to come into the world
}

# JSON-RPC's own error codes, then the API's (RESIDENT-API.md, "Errors")
PARSE_ERROR, INVALID_REQUEST, METHOD_NOT_FOUND, INVALID_PARAMS = -32700, -32600, -32601, -32602
NOT_SIGNED_IN, BAD_KEY, NOT_ALLOWED_HERE, PAUSED, RATE_LIMITED = 1001, 1002, 1003, 1004, 1005
UNKNOWN_PERSON, AMBIGUOUS, OUT_OF_WORLD, FETCH_FAILED, UNSUPPORTED_VERSION = 1006, 1007, 1008, 1009, 1010
BODY_UNAVAILABLE, KEY_CHECK_FAILED = 1011, 1012
ERROR_NAMES = {NOT_SIGNED_IN: "not_signed_in", BAD_KEY: "bad_key", NOT_ALLOWED_HERE: "not_allowed_here",
               PAUSED: "paused", RATE_LIMITED: "rate_limited", UNKNOWN_PERSON: "unknown_person",
               AMBIGUOUS: "ambiguous", OUT_OF_WORLD: "out_of_world", FETCH_FAILED: "fetch_failed",
               UNSUPPORTED_VERSION: "unsupported_version", BODY_UNAVAILABLE: "body_unavailable",
               KEY_CHECK_FAILED: "key_check_failed"}
CODES_BY_NAME = {v: k for k, v in ERROR_NAMES.items()}

VIEW_RADIUS = 50.0                                   # an agent sees avatars this far (Agent.avatarViewRadius)
WATCH_RATE_MS = 500                                  # the body's perception while a program watches near/left only
DRIVEN_RATE_MS = 1000                                # while a program drives it without watching (names to ids)


class RpcError(Exception):
    def __init__(self, code: int, message: str, data: dict | None = None):
        super().__init__(message)
        self.code, self.message, self.data = code, message, data

    def to_json(self) -> dict:
        err = {"code": self.code, "message": self.message}
        data = dict(self.data or {})
        if self.code in ERROR_NAMES:
            data.setdefault("name", ERROR_NAMES[self.code])
        if data:
            err["data"] = data
        return err


def load_config(path: Path | None) -> dict:
    cfg = json.loads(json.dumps(DEFAULTS))
    if path is not None:
        with open(path, encoding="utf-8") as f:
            given = json.load(f)
        for k, v in given.items():
            if isinstance(v, dict) and isinstance(cfg.get(k), dict):
                cfg[k].update(v)
            else:
                cfg[k] = v
    base = path.resolve().parent if path is not None else HERE
    cfg["_base"] = str(base)
    cfg["residents"] = [str(r).lower() for r in cfg["residents"]]
    return cfg


def resolve(cfg: dict, p: str) -> Path:
    q = Path(p)
    return q if q.is_absolute() else Path(cfg["_base"]) / q


# ---- checking params -------------------------------------------------------------------------------------------

UUID_RE = re.compile(r"^\{?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\}?$")
NAME_RE = re.compile(r"^[A-Za-z0-9_]{1,64}$")
SPEEDS = {"walk": 1.3, "run": 3.5}


def _bad(msg: str):
    raise RpcError(INVALID_PARAMS, msg)


def _number(v, name: str, lo: float | None = None, hi: float | None = None) -> float:
    if isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v):
        _bad(f"{name} must be a number")
    if (lo is not None and v < lo) or (hi is not None and v > hi):
        _bad(f"{name} must be between {lo} and {hi}")
    return float(v)


def _position(v, name: str = "position") -> list:
    if not isinstance(v, list) or len(v) != 3:
        _bad(f"{name} must be [x, y, z]")
    return [_number(c, name, -1e6, 1e6) for c in v]


def _text(v, name: str, most: int, least: int = 1) -> str:
    if not isinstance(v, str) or not (least <= len(v) <= most):
        _bad(f"{name} must be text of {least} to {most} characters")
    return v


def _url(v, name: str = "url") -> str:
    _text(v, name, 2048)
    if urllib.parse.urlsplit(v).scheme not in ("http", "https"):
        _bad(f"{name} must be an http or https address")
    return v


def _speed(v, limits: dict) -> float:
    if v is None:
        return min(SPEEDS["walk"], limits["speed_max"])
    if isinstance(v, str):
        if v not in SPEEDS:
            _bad('speed must be "walk", "run" or a number in m/s')
        return min(SPEEDS[v], limits["speed_max"])
    return _number(v, "speed", 0.1, limits["speed_max"])


def _target(p: dict, allowed: tuple) -> dict:
    """Exactly one of person / position / yaw (those allowed), checked."""
    given = [k for k in allowed if p.get(k) is not None]
    if len(given) != 1:
        _bad("give one of " + ", ".join(allowed))
    k = given[0]
    if k == "person":
        return {"person": _text(p["person"], "person", 64)}
    if k == "position":
        return {"position": _position(p["position"])}
    return {"yaw": _number(p["yaw"], "yaw", -360, 360)}


def check_params(method: str, p: dict, limits: dict) -> dict:
    """The params that go on: only known fields, each checked. Raises RpcError(INVALID_PARAMS)."""
    if method == "go_to":
        out = _target(p, ("person", "position"))
        out["within"] = _number(p.get("within", 1.2), "within", 0.3, 50)
        out["speed"] = _speed(p.get("speed"), limits)
        return out
    if method == "route":
        pts = p.get("points")
        if not isinstance(pts, list) or not 1 <= len(pts) <= 100:
            _bad("points must be a list of 1 to 100 positions")
        return {"points": [_position(q, "points") for q in pts], "speed": _speed(p.get("speed"), limits),
                "pause": _number(p.get("pause", 0), "pause", 0, 600)}
    if method == "follow":
        return {"person": _text(p.get("person"), "person", 64),
                "distance": _number(p.get("distance", 1.5), "distance", 0.5, 30), "speed": _speed(p.get("speed"), limits)}
    if method == "face":
        return _target(p, ("person", "position", "yaw"))
    if method in ("stop", "stop_line", "look_away", "gestures", "stop_animation"):
        return {}
    if method == "say":
        return {"text": _text(p.get("text"), "text", 500)}
    if method == "play_line":
        return {"url": _url(p.get("url")), "volume": _number(p.get("volume", 1.0), "volume", 0, 1)}
    if method == "look_at":
        out = _target(p, ("person", "position"))
        out["eyes_only"] = bool(p.get("eyes_only", False))
        return out
    if method == "gesture":
        return {"name": _text(p.get("name"), "name", 64)}
    if method == "animate":
        return {"url": _url(p.get("url")), "loop": bool(p.get("loop", True)),
                "fps": _number(p.get("fps", 30), "fps", 1, 120)}
    if method == "look":
        return {"radius": _number(p.get("radius", 25), "radius", 1, VIEW_RADIUS)}
    if method == "watch":
        every = int(_number(p.get("every", 0), "every", 0, 5000))
        if every and every < 250:
            _bad("every must be 0 or 250 to 5000 ms")
        return {"radius": _number(p.get("radius", 10), "radius", 1, VIEW_RADIUS), "every": every}
    if method == "cue":
        entity = _text(p.get("entity"), "entity", 38)
        if not UUID_RE.match(entity):
            _bad("entity must be an entity id")
        params = p.get("params", [])
        if not isinstance(params, list) or len(params) > 16:
            _bad("params must be a list of up to 16 strings")
        # the engine passes a cue's params as strings: numbers are turned into strings here, as they would be there
        params = [json.dumps(x) if isinstance(x, (int, float)) and not isinstance(x, bool) else x for x in params]
        return {"entity": entity, "method": _text(p.get("method"), "method", 64),
                "params": [_text(x, "params", 1024, 0) for x in params]}
    if method == "send":
        return {"channel": _text(p.get("channel"), "channel", 128), "message": _text(p.get("message"), "message", 4096, 0)}
    if method == "listen":
        return {"channel": _text(p.get("channel"), "channel", 128), "off": bool(p.get("off", False))}
    raise RpcError(METHOD_NOT_FOUND, f"no method {method!r}")


BODY_METHODS = {"go_to", "route", "follow", "face", "stop", "say", "play_line", "stop_line", "look_at", "look_away",
                "gesture", "animate", "stop_animation", "cue", "send", "listen"}
SERVICE_METHODS = {"watch", "look", "gestures"}       # answered by the service from what the body last told it
MOVEMENTS = {"go_to", "route", "follow"}
LONG_RUNNING = MOVEMENTS | {"play_line", "animate"}
# events a body sends that carry a request's id as `re` (RESIDENT-API.md, "Events")
EVENTS_WITH_RE = {"arrived", "route_finished", "lost", "line_finished", "failed"}


# ---- limits ----------------------------------------------------------------------------------------------------

class Bucket:
    """A token bucket: `size` at most, refilled at `rate` a second."""

    def __init__(self, size: float, rate: float, clock=time.monotonic):
        self.size, self.rate, self.clock = size, rate, clock
        self.tokens, self.at = size, clock()

    def take(self) -> float:
        """0 when allowed, else the seconds until one is free."""
        now = self.clock()
        self.tokens = min(self.size, self.tokens + (now - self.at) * self.rate)
        self.at = now
        if self.tokens >= 1:
            self.tokens -= 1
            return 0.0
        return (1 - self.tokens) / self.rate


# ---- the key check ---------------------------------------------------------------------------------------------

def check_key(metaverse: str, resident: str, key: str, timeout: float = 15) -> dict:
    """The metaverse's own check of a resident key (the client-credentials grant the body signs in with), then the
    account it belongs to. Returns {"username", "kind", "host"}; raises RpcError(BAD_KEY, PAUSED or
    KEY_CHECK_FAILED)."""
    base = metaverse.rstrip("/")
    body = urllib.parse.urlencode({"grant_type": "client_credentials", "client_id": resident,
                                   "client_secret": key, "scope": "owner"}).encode()
    req = urllib.request.Request(base + "/oauth/token", data=body, method="POST",
                                 headers={"Content-Type": "application/x-www-form-urlencoded",
                                          "User-Agent": "Darkride resident service"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            token = json.loads(r.read().decode("utf-8")).get("access_token")
    except urllib.error.HTTPError as e:
        reason = ""
        try:
            j = json.loads(e.read().decode("utf-8"))
            reason = str(j.get("error_description") or j.get("message") or j.get("error") or "")
        except (ValueError, OSError):
            pass
        finally:
            e.close()
        if "paused" in reason.lower():
            raise RpcError(PAUSED, "the resident is paused by its host", {"reason": "host"})
        raise RpcError(BAD_KEY, "the key was refused" + (f": {reason}" if reason else ""))
    except (urllib.error.URLError, OSError, ValueError) as e:
        raise RpcError(KEY_CHECK_FAILED, f"the key couldn't be checked just now: {e}")
    if not token:
        raise RpcError(BAD_KEY, "the key was refused")
    req = urllib.request.Request(base + "/api/v1/user/profile",
                                 headers={"Authorization": "Bearer " + token, "User-Agent": "Darkride resident service"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            user = (json.loads(r.read().decode("utf-8")).get("data") or {}).get("user") or {}
    except (urllib.error.URLError, OSError, ValueError) as e:
        raise RpcError(KEY_CHECK_FAILED, f"the account couldn't be read just now: {e}")
    kind = user.get("kind") or ("ai" if user.get("ai") else "person")   # `kind` first: a bot also says ai: true
    if kind not in ("ai", "bot") or not user.get("username"):
        raise RpcError(BAD_KEY, "this key isn't a resident's")
    return {"username": str(user["username"]), "kind": kind, "host": user.get("host")}


# ---- bodies ----------------------------------------------------------------------------------------------------

class ProcessLauncher:
    """Starts a body: one assignment-client signed in as the resident, taking that resident's pool's script (the
    domain's persistent script for the pool points at /residents/body.js?resident=<name> here). Stops it by its
    recorded pid, never by name. The pids are also kept in <logs>/bodies.json, so bodies left by a service that died
    are stopped when it starts again (only if that pid is still this resident's assignment-client)."""

    def __init__(self, cfg: dict):
        self.cfg = cfg
        self.logs = resolve(cfg, cfg["logs"])
        self.record = self.logs / "bodies.json"

    def assignment_client(self) -> str:
        if self.cfg["assignment_client"]:
            return str(resolve(self.cfg, self.cfg["assignment_client"]))
        exe = "assignment-client.exe" if os.name == "nt" else "assignment-client"
        return str(Path(self.cfg["_base"]).parent / exe)

    def _read(self) -> dict:
        try:
            return json.loads(self.record.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return {}

    def _write(self, pids: dict):
        self.logs.mkdir(parents=True, exist_ok=True)
        tmp = self.record.with_suffix(".tmp")
        tmp.write_text(json.dumps(pids, indent=1), encoding="utf-8")
        os.replace(tmp, self.record)

    def start(self, resident: str, key: str):
        self.logs.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, HIFI_AC_USERNAME=resident, HIFI_AC_KEY=key, HIFI_METAVERSE_URL=self.cfg["metaverse"])
        world = self.cfg["world"]
        args = [self.assignment_client(), "-t", "2", "-a", str(world["address"]),
                "--server-port", str(world["udp_port"]), "--pool", resident]
        out = open(self.logs / f"{resident}.log", "ab")
        kw = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
        try:
            proc = subprocess.Popen(args, env=env, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT, **kw)
        finally:
            out.close()
        proc.resident = resident
        pids = self._read()
        pids[str(proc.pid)] = resident
        self._write(pids)
        log.info("body of %s started, pid %d", resident, proc.pid)
        return proc

    @staticmethod
    def alive(proc) -> bool:
        return proc.poll() is None

    async def stop(self, proc, wait: float = 10.0):
        if proc.poll() is None:
            try:
                if os.name == "nt":
                    proc.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    proc.terminate()
            except OSError:
                pass
            deadline = time.monotonic() + wait
            while proc.poll() is None and time.monotonic() < deadline:
                await asyncio.sleep(0.1)
            if proc.poll() is None:
                proc.kill()
            log.info("body pid %d stopped", proc.pid)
        pids = self._read()
        if pids.pop(str(proc.pid), None) is not None:
            self._write(pids)

    @staticmethod
    def is_body(pid: int, resident: str) -> bool:
        """Whether pid is still an assignment-client of that resident's pool (a pid can be reused after a reboot)."""
        try:
            if os.name == "nt":
                out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
                                     capture_output=True, text=True, timeout=10).stdout
                return out.strip().lower().startswith('"assignment-client.exe"')
            out = subprocess.run(["ps", "-o", "command=", "-p", str(pid)],
                                 capture_output=True, text=True, timeout=10).stdout
            return "assignment-client" in out and f"--pool {resident}" in out
        except (OSError, subprocess.SubprocessError):
            return False

    def stop_leftovers(self):
        """Bodies a service that died left running: stopped by their recorded pid, once checked."""
        for pid_text, resident in self._read().items():
            pid = int(pid_text)
            if not self.is_body(pid, resident):
                continue
            log.warning("stopping %s's body left from before, pid %d", resident, pid)
            try:
                if os.name == "nt":
                    subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, timeout=10)
                else:
                    os.kill(pid, signal.SIGTERM)
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline and self.is_body(pid, resident):
                        time.sleep(0.2)
                    if self.is_body(pid, resident):
                        os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
        self._write({})


class Body:
    """One resident's body: its process, its link to the service, what it last perceived, and who drives it."""

    def __init__(self, resident: str):
        self.resident = resident
        self.token: str | None = None                # one per start, in the body script served only while the body
        self.proc = None                              # is starting; never logged
        self.link = None                              # the body's WebSocket, once it has said hello
        self.me: dict = {}                            # {"id", "position", "yaw"}
        self.gestures: list = []
        self.people: list = []                        # the latest perception: [{"id", "name", "kind", "position", ...}]
        self.joined = asyncio.Event()                 # set by the body's `ready`
        self.driver: Session | None = None
        self.grace: asyncio.Task | None = None
        self.rate: int | None = None                  # the perception rate last asked of the body, ms
        self.leaving = False                          # the service asked it to leave
        self.next_id = 0
        self.pending: dict = {}                       # body request id -> (future, session)
        self.long_running: dict = {}                  # body request id -> (session, program id, method), for `re`

    def person(self, pid) -> dict:
        for q in self.people:
            if q.get("id") == pid:
                return _person(q)
        return {"id": pid, "name": None, "kind": "person"}

    def distance_to(self, pid) -> float | None:
        here = self.me.get("position")
        for q in self.people:
            if q.get("id") == pid and _is_position(q.get("position")) and _is_position(here):
                return round(math.dist(here, q["position"]), 2)
        return None


def _is_position(v) -> bool:
    return isinstance(v, list) and len(v) == 3 and all(isinstance(c, (int, float)) for c in v)


def _person(q: dict) -> dict:
    kind = q.get("kind") if q.get("kind") in ("person", "ai", "bot") else "person"
    return {"id": q.get("id"), "name": q.get("name"), "kind": kind}


def _norm_id(s: str) -> str:
    return s.strip("{}").lower()


def find_person(people: list, who: str) -> str:
    """A person's session id, from a session id or a display name. Raises RpcError(UNKNOWN_PERSON or AMBIGUOUS)."""
    for q in people:
        if isinstance(q.get("id"), str) and _norm_id(q["id"]) == _norm_id(who):
            return q["id"]
    matches = [q for q in people if str(q.get("name") or "").lower() == who.lower()]
    if len(matches) == 1:
        return matches[0]["id"]
    if not matches:
        raise RpcError(UNKNOWN_PERSON, f"nobody called {who!r} is in the world")
    raise RpcError(AMBIGUOUS, f"{len(matches)} people are called {who!r}: use an id",
                   {"people": [_person(q) for q in matches]})


# ---- one program's connection ----------------------------------------------------------------------------------

class Session:
    def __init__(self, service: "Service", ws):
        self.service, self.ws = service, ws
        self.resident: str | None = None
        self.account: dict = {}
        limits = service.cfg["limits"]
        self.requests = Bucket(limits["requests_per_second"], limits["requests_per_second"])
        self.says = Bucket(limits["say_per_minute"], limits["say_per_minute"] / 60.0)
        self.watch = None                             # {"radius", "every", "near": set(), "last_people": 0}
        self.closed = False

    async def send(self, msg: dict):
        if self.closed:
            return
        try:
            await self.ws.send(json.dumps(msg, separators=(",", ":")))
        except ConnectionClosed:
            self.closed = True

    async def notify(self, method: str, params: dict):
        await self.send({"jsonrpc": "2.0", "method": method, "params": params})

    async def end(self, reason: str):
        await self.notify("ended", {"reason": reason})
        self.closed = True
        try:
            await self.ws.close(1000, reason)
        except ConnectionClosed:
            pass

    async def run(self):
        try:
            first = await asyncio.wait_for(self.ws.recv(), timeout=10)
        except (asyncio.TimeoutError, ConnectionClosed):
            return
        await self.handle_text(first)
        async for text in self.ws:                    # one at a time, in order: "stop" after "go_to" means it
            await self.handle_text(text)

    async def handle_text(self, text):
        msg_id = None
        try:
            try:
                msg = json.loads(text)
            except (ValueError, TypeError):
                raise RpcError(PARSE_ERROR, "not JSON")
            if not isinstance(msg, dict):
                raise RpcError(INVALID_REQUEST, "a request is a JSON object")
            msg_id = msg.get("id")
            if msg.get("jsonrpc") != "2.0" or not isinstance(msg.get("method"), str) \
                    or not (msg_id is None or isinstance(msg_id, (int, str))) or isinstance(msg_id, bool):
                msg_id = None
                raise RpcError(INVALID_REQUEST, "not a JSON-RPC 2.0 request")
            params = msg.get("params", {})
            if not isinstance(params, dict):
                raise RpcError(INVALID_PARAMS, "params must be an object")
            result = await self.call(msg["method"], params, msg_id)
            if msg_id is not None:
                await self.send({"jsonrpc": "2.0", "id": msg_id, "result": result})
        except RpcError as e:
            await self.send({"jsonrpc": "2.0", "id": msg_id, "error": e.to_json()})

    async def call(self, method: str, p: dict, msg_id=None):
        if method == "hello":
            return await self.hello(p)
        if self.resident is None:
            raise RpcError(NOT_SIGNED_IN, "send hello first")
        if method not in BODY_METHODS and method not in SERVICE_METHODS:
            raise RpcError(METHOD_NOT_FOUND, f"no method {method!r}")
        wait = self.requests.take()
        if wait:
            raise RpcError(RATE_LIMITED, "too many requests", {"retry_after": round(wait, 2)})
        clean = check_params(method, p, self.service.cfg["limits"])
        body = self.service.driven_body(self)
        if method == "watch":
            self.watch = dict(clean, near=set(), last_people=0.0)
            await self.service.set_rate(body)
            return {}
        if method == "gestures":
            return {"names": list(body.gestures)}
        if method == "look":
            return self.look(body, clean["radius"])
        if method == "say":
            wait = self.says.take()
            if wait:
                raise RpcError(RATE_LIMITED, "too many chat lines", {"retry_after": round(wait, 2)})
            self.service.safety.check(self, clean["text"])
        if "person" in clean:                         # the body deals in session ids only
            clean["person"] = find_person(body.people, clean["person"])
        return await self.service.to_body(self, body, method, clean, msg_id)

    def look(self, body: Body, radius: float) -> dict:
        here = body.me.get("position")
        near = []
        for q in body.people:
            if _is_position(q.get("position")) and _is_position(here):
                d = math.dist(here, q["position"])
                if d <= radius:
                    near.append({"person": _person(q), "position": q["position"], "distance": round(d, 2),
                                 "yaw": q.get("yaw"), "looking_at": q.get("lookAt")})
        near.sort(key=lambda e: e["distance"])
        return {"me": body.me, "people": near}

    async def hello(self, p: dict):
        if self.resident is not None:
            raise RpcError(INVALID_REQUEST, "already signed in")
        if p.get("v") != API_VERSION:
            raise RpcError(UNSUPPORTED_VERSION, f"this service speaks version {API_VERSION}", {"v": API_VERSION})
        resident = _text(p.get("resident"), "resident", 64)
        key = _text(p.get("key"), "key", 512)
        if not NAME_RE.match(resident):
            _bad("resident must be a resident's username")
        account = await asyncio.to_thread(self.service.check_key, self.service.cfg["metaverse"], resident, key)
        name = account["username"].lower()
        if name not in self.service.cfg["residents"]:
            raise RpcError(NOT_ALLOWED_HERE, "this world's owner hasn't let this resident in")
        self.account = account
        body = await self.service.attach(self, name, key)
        if body.driver is not self:                   # another hello for this resident came while the body started
            raise RpcError(BODY_UNAVAILABLE, "replaced by a newer connection")
        self.resident = name
        limits = self.service.cfg["limits"]
        return {"v": API_VERSION, "resident": name, "kind": account["kind"], "host": account.get("host"),
                "world": self.service.world_info(), "me": body.me,
                "limits": {"say_per_minute": limits["say_per_minute"],
                           "requests_per_second": limits["requests_per_second"], "speed_max": limits["speed_max"]}}

    async def perceived(self, body: Body):
        """The body's latest perception: works out near, left and people for this program's watch."""
        w = self.watch
        here = body.me.get("position")
        if w is None or not _is_position(here):
            return
        inside = {}
        for q in body.people:
            if q.get("id") and _is_position(q.get("position")):
                d = math.dist(here, q["position"])
                if d <= w["radius"]:
                    inside[q["id"]] = (q, d)
        for pid, (q, d) in inside.items():
            if pid not in w["near"]:
                await self.notify("near", {"person": _person(q), "distance": round(d, 2)})
        for pid in [pid for pid in w["near"] if pid not in inside]:
            await self.notify("left", {"person": body.person(pid)})
        w["near"] = set(inside)
        now = time.monotonic()
        if w["every"] and inside and (now - w["last_people"]) * 1000 >= w["every"] - 50:
            w["last_people"] = now
            await self.notify("people", {"people": [
                {"person": _person(q), "position": q["position"], "distance": round(d, 2),
                 "facing_me": q.get("facing_me")} for q, d in inside.values()]})


class NoFilter:
    """Where the safety filter goes: it will check what an AI resident says and hold
    what crosses the line. Until it exists nothing is held; a bot's scripted lines are its host's own."""

    def check(self, session: Session, text: str):
        return None


# ---- the service -----------------------------------------------------------------------------------------------

class Service:
    def __init__(self, cfg: dict, launcher=None, key_checker=check_key, safety=None):
        self.cfg = cfg
        self.launcher = launcher or ProcessLauncher(cfg)
        self.check_key = key_checker
        self.safety = safety or NoFilter()
        self.bodies: dict = {}                        # resident -> Body
        self.stopping = asyncio.Event()

    def world_info(self) -> dict:
        w = self.cfg["world"]
        return {"address": w["address"], "udp_port": w["udp_port"], "name": w.get("name")}

    def driven_body(self, session: Session) -> Body:
        body = self.bodies.get(session.resident or "")
        if body is None or body.link is None or body.driver is not session:
            raise RpcError(BODY_UNAVAILABLE, "the body isn't in the world")
        return body

    # -- programs --
    async def attach(self, session: Session, resident: str, key: str) -> Body:
        body = self.bodies.get(resident)
        if body is None:
            body = self.bodies[resident] = Body(resident)
        if body.grace is not None:
            body.grace.cancel()
            body.grace = None
        old, body.driver = body.driver, session
        if old is not None and old is not session:
            old.watch = None
            await old.end("replaced")
        if body.proc is None or not self.launcher.alive(body.proc):
            body.joined.clear()
            body.link, body.rate = None, None
            body.token = secrets.token_urlsafe(24)
            try:
                body.proc = self.launcher.start(resident, key)
            except OSError as e:
                log.error("the body of %s couldn't start: %s", resident, e)
                body.driver = None
                await self.stop_body(body)
                raise RpcError(BODY_UNAVAILABLE, "the body couldn't start (see the service's logs)")
        try:
            await asyncio.wait_for(body.joined.wait(), timeout=self.cfg["body_start_seconds"])
        except asyncio.TimeoutError:
            if body.driver is session:
                body.driver = None
                await self.stop_body(body)
            raise RpcError(BODY_UNAVAILABLE, "the body didn't come into the world (see the service's logs)")
        await self.set_rate(body)
        return body

    async def detach(self, session: Session):
        body = self.bodies.get(session.resident or "")
        if body is None or body.driver is not session:
            return
        body.driver = None
        for fut, s in list(body.pending.values()):
            if s is session and not fut.done():
                fut.set_exception(RpcError(BODY_UNAVAILABLE, "the program went away"))
        body.long_running = {k: v for k, v in body.long_running.items() if v[0] is not session}
        await self.set_rate(body)
        body.grace = asyncio.create_task(self._grace(body))

    async def _grace(self, body: Body):
        try:
            await asyncio.sleep(self.cfg["body_grace_seconds"])
        except asyncio.CancelledError:
            return
        body.grace = None
        if body.driver is None:
            log.info("nobody drives %s: its body leaves", body.resident)
            await self.stop_body(body)

    async def set_rate(self, body: Body):
        """Asks the body for perception only as often as its program needs it (none while nobody drives it)."""
        s = body.driver
        if s is None:
            rate = 0
        elif s.watch is not None:
            rate = s.watch["every"] or WATCH_RATE_MS
        else:
            rate = DRIVEN_RATE_MS
        if rate != body.rate and body.link is not None:
            body.rate = rate
            await self.body_request(body, "perceive", {"every": rate})

    async def body_request(self, body: Body, method: str, params: dict):
        """A request of the service's own; its answer isn't awaited."""
        body.next_id += 1
        try:
            await body.link.send(json.dumps({"jsonrpc": "2.0", "id": body.next_id, "method": method, "params": params}))
        except (ConnectionClosed, AttributeError):
            pass

    async def stop_body(self, body: Body):
        link = body.link
        if link is not None:
            body.leaving = True
            await self.body_request(body, "leave", {})        # the body stops being an avatar and ends its script
            try:
                await asyncio.wait_for(link.wait_closed(), 2)
            except asyncio.TimeoutError:
                pass
            try:
                await link.close(1000, "leaving")
            except ConnectionClosed:
                pass
        if body.proc is not None:
            await self.launcher.stop(body.proc)
        body.proc, body.link, body.token = None, None, None
        body.leaving = False
        body.joined.clear()
        if self.bodies.get(body.resident) is body and body.driver is None:
            del self.bodies[body.resident]

    async def to_body(self, session: Session, body: Body, method: str, params: dict, program_id):
        body.next_id += 1
        bid = body.next_id
        fut = asyncio.get_running_loop().create_future()
        body.pending[bid] = (fut, session)
        if method in LONG_RUNNING:
            # registered before sending, so an event that comes before the reply (a line that can't load) isn't lost
            body.long_running[bid] = (session, program_id, method)
        try:
            await body.link.send(json.dumps({"jsonrpc": "2.0", "id": bid, "method": method, "params": params}))
            reply = await asyncio.wait_for(fut, timeout=30)
        except (ConnectionClosed, RpcError, asyncio.TimeoutError) as e:
            body.long_running.pop(bid, None)
            if isinstance(e, RpcError):
                raise
            raise RpcError(BODY_UNAVAILABLE, "the body didn't answer" if isinstance(e, asyncio.TimeoutError)
                           else "the body went away")
        finally:
            body.pending.pop(bid, None)
        if "error" in reply:
            e = reply["error"] if isinstance(reply["error"], dict) else {}
            body.long_running.pop(bid, None)
            code = e.get("code") if isinstance(e.get("code"), int) else BODY_UNAVAILABLE
            raise RpcError(code, str(e.get("message", "the body refused")), e.get("data"))
        # a request that replaces or ends older ones: those send no more events (the body has already sent any last
        # one, such as a replaced route's route_finished), so their entries go
        ended = MOVEMENTS if method in MOVEMENTS or method == "stop" else \
            {"animate"} if method in ("animate", "stop_animation") else set()
        for old in [k for k, v in body.long_running.items() if k < bid and v[2] in ended]:
            del body.long_running[old]
        return reply.get("result", {})

    # -- bodies --
    async def body_link(self, ws):
        """A body's link: its first message is body_hello with its resident and the token in the script it got."""
        try:
            first = json.loads(await asyncio.wait_for(ws.recv(), timeout=10))
        except (asyncio.TimeoutError, ConnectionClosed, ValueError, TypeError):
            return
        first = first if isinstance(first, dict) else {}
        p = first.get("params") if isinstance(first.get("params"), dict) else {}
        body = self.bodies.get(str(p.get("resident", "")).lower())
        if first.get("method") != "body_hello" or body is None or body.token is None \
                or not secrets.compare_digest(str(p.get("token", "")), body.token):
            await ws.send(json.dumps({"jsonrpc": "2.0", "id": first.get("id"),
                                      "error": {"code": NOT_SIGNED_IN, "message": "unknown body"}}))
            return
        body.token = None                             # used: nobody else can fetch it or sign in with it
        old, body.link = body.link, ws                # the new link first, so the old one's ending isn't taken for
        if old is not None:                           # the body going away
            try:
                await old.close(1000, "replaced")
            except ConnectionClosed:
                pass
        await ws.send(json.dumps({"jsonrpc": "2.0", "id": first.get("id"), "result": {"v": API_VERSION}}))
        try:
            async for text in ws:
                await self.from_body(body, ws, text)
        except ConnectionClosed:
            pass
        finally:
            if body.link is ws:
                body.link = None
                body.joined.clear()
                for fut, _ in body.pending.values():
                    if not fut.done():
                        fut.set_exception(RpcError(BODY_UNAVAILABLE, "the body went away"))
                if body.driver is not None and not self.stopping.is_set():
                    log.warning("the body of %s went away", body.resident)
                    await body.driver.end("body_stopped")
                    body.driver = None
                    await self.stop_body(body)

    async def from_body(self, body: Body, ws, text):
        try:
            msg = json.loads(text)
        except (ValueError, TypeError):
            return
        if not isinstance(msg, dict):
            return
        if "id" in msg and ("result" in msg or "error" in msg):
            entry = body.pending.get(msg["id"])
            if entry and not entry[0].done():
                entry[0].set_result(msg)
            return
        method = msg.get("method")
        p = msg.get("params") if isinstance(msg.get("params"), dict) else {}
        driver = body.driver
        if method == "ready":                         # in the world, its identity with the avatar mixer
            body.me = {"id": p.get("id"), "position": p.get("position"), "yaw": p.get("yaw")}
            body.gestures = [g for g in p.get("gestures") or [] if isinstance(g, str)]
            if isinstance(p.get("people"), list):
                body.people = p["people"]
            body.joined.set()
            log.info("body of %s is in the world", body.resident)
            return
        if method == "perception":
            if isinstance(p.get("me"), dict):
                body.me = dict(body.me, **p["me"])
            if isinstance(p.get("people"), list):
                body.people = p["people"]
            if driver is not None:
                await driver.perceived(body)
            return
        if method == "bye":                           # leaving: asked to, or on its own (domain lost, kicked)
            (log.info if body.leaving else log.warning)("the body of %s is leaving: %s", body.resident, p.get("reason"))
            await ws.close(1000, "bye")
            return
        if driver is None:
            return
        if method in EVENTS_WITH_RE:
            entry = body.long_running.get(p.get("re"))
            if entry is None or entry[0] is not driver:
                return
            done = method != "arrived" or entry[2] != "route"
            if done:
                body.long_running.pop(p.get("re"), None)
            if method == "failed":
                code = CODES_BY_NAME.get(str(p.get("code")), BODY_UNAVAILABLE)
                p = {"re": entry[1], "error": {"code": code, "name": ERROR_NAMES[code]}}
            else:
                p = dict(p, re=entry[1])              # the program's own request id
            await driver.notify(method, p)
            return
        if method == "heard":
            await driver.notify("heard", {"person": body.person(p.get("id")), "text": str(p.get("text", "")),
                                          "distance": body.distance_to(p.get("id"))})
            return
        if method == "message":
            sender = p.get("from")
            await driver.notify("message", {"channel": p.get("channel"), "message": p.get("message"),
                                            "from": body.person(sender) if sender else None})

    # -- the body script --
    def body_script(self, resident: str) -> str | None:
        """The body script with its service, resident and one-time token written in; only while that body starts."""
        body = self.bodies.get(resident.lower())
        if body is None or body.token is None:
            return None
        src = resolve(self.cfg, self.cfg["body_script"]).read_text(encoding="utf-8")
        url = f"ws://127.0.0.1:{self.cfg['port']}/residents/body"
        anim = f"http://127.0.0.1:{self.cfg['port']}/residents/anim/"
        avatar = str((self.cfg.get("avatars") or {}).get(body.resident, ""))   # public: every visitor loads it
        return (src.replace("__SERVICE_URL__", url).replace("__RESIDENT__", body.resident)
                .replace("__TOKEN__", body.token).replace("__ANIM_URL__", anim).replace("__AVATAR_URL__", avatar))

    def animation(self, name: str) -> bytes | None:
        """A clip from the animations folder, for the bodies on this computer only (they alone load clips)."""
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,128}", name) or name.startswith(".") \
                or not name.lower().endswith((".fbx", ".json", ".glb", ".gltf")):
            return None
        folder = resolve(self.cfg, self.cfg["animations"]).resolve()
        path = (folder / name).resolve()
        if path.parent != folder or not path.is_file():
            return None
        return path.read_bytes()

    async def http(self, connection, request):
        """Plain HTTP on the same port: the body script for a resident's pool (GET /residents/body.js?resident=...)."""
        parts = urllib.parse.urlsplit(request.path)
        if parts.path == "/residents/body.js":
            resident = (urllib.parse.parse_qs(parts.query).get("resident") or [""])[0]
            try:
                text = self.body_script(resident)
            except OSError as e:
                log.error("the body script can't be read: %s", e)
                return connection.respond(500, "the body script can't be read\n")
            if text is None:
                return connection.respond(404, "no body of that resident is starting here\n")
            return connection.respond(200, text)
        if parts.path.startswith("/residents/anim/"):
            data = self.animation(urllib.parse.unquote(parts.path[len("/residents/anim/"):]))
            if data is None:
                return connection.respond(404, "no such clip\n")
            return Response(200, "OK", Headers([("Content-Type", "application/octet-stream"),
                                                 ("Content-Length", str(len(data)))]), data)
        if parts.path not in ("/residents/v1", "/residents/body"):
            return connection.respond(404, "not found\n")
        return None

    async def handler(self, ws):
        path = urllib.parse.urlsplit(ws.request.path).path
        if path == "/residents/body":
            await self.body_link(ws)
            return
        session = Session(self, ws)
        try:
            await session.run()
        except ConnectionClosed:
            pass
        finally:
            session.closed = True
            await self.detach(session)

    async def run(self, ready: asyncio.Event | None = None):
        leftovers = getattr(self.launcher, "stop_leftovers", None)
        if leftovers is not None:
            await asyncio.to_thread(leftovers)
        async with serve(self.handler, "127.0.0.1", self.cfg["port"], process_request=self.http,
                         max_size=256 * 1024, ping_interval=5, ping_timeout=20) as server:
            self.server = server
            log.info("listening on ws://127.0.0.1:%d/residents/v1 for %s", self.cfg["port"],
                     ", ".join(self.cfg["residents"]) or "nobody yet (config: residents)")
            if ready is not None:
                ready.set()
            await self.stopping.wait()
            for body in list(self.bodies.values()):
                if body.driver is not None:
                    await body.driver.end("world_closed")
                    body.driver = None
                await self.stop_body(body)


def main(argv=None):
    ap = argparse.ArgumentParser(description="The resident service (RESIDENT-API.md)")
    ap.add_argument("--config", type=Path, default=HERE / "config.json" if (HERE / "config.json").exists() else None)
    args = ap.parse_args(argv)
    cfg = load_config(args.config)
    logs = resolve(cfg, cfg["logs"])
    logs.mkdir(parents=True, exist_ok=True)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s",
                        handlers=[logging.FileHandler(logs / "resident-service.log"), logging.StreamHandler()])
    service = Service(cfg)

    async def go():
        loop = asyncio.get_running_loop()
        for name in ("SIGTERM", "SIGINT", "SIGBREAK"):
            sig = getattr(signal, name, None)
            if sig is None:
                continue
            try:
                loop.add_signal_handler(sig, service.stopping.set)
            except (NotImplementedError, RuntimeError):
                signal.signal(sig, lambda *_: loop.call_soon_threadsafe(service.stopping.set))
        await service.run()

    asyncio.run(go())


if __name__ == "__main__":
    main()
