#
#  test_resident_service.py
#  resident-service
#
#  Checks for resident_service.py without the engine: a stand-in body (it fetches the body script the service serves,
#  signs in on the body link with the one-time token in it, and answers like resident-body.js), a stand-in key check,
#  and a stand-in metaverse for the real key check.
#
#      python3 -m unittest test_resident_service.py
#
#  Copyright 2026 Darkride.
#
#  Distributed under the MIT License.
#

import asyncio
import json
import os
import re
import socket
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from websockets.asyncio.client import connect

import resident_service as RS

VISITOR = {"id": "{aaaaaaaa-0000-0000-0000-000000000001}", "name": "Visitor", "kind": "person",
           "position": [3, 0, 0], "yaw": 180}
SAM_1 = {"id": "s-1", "name": "Sam", "position": [20, 0, 0]}
SAM_2 = {"id": "s-2", "name": "sam", "position": [30, 0, 0]}


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def get(url, raw=False):
    try:
        with urllib.request.urlopen(url, timeout=5) as r:
            data = r.read()
    except urllib.error.HTTPError as e:
        e.close()
        raise
    return data if raw else data.decode()


class FakeProc:
    def __init__(self, task):
        self.task = task
        self.stopped = False


class FakeBody:
    """Stands in for resident-body.js in an assignment-client, speaking the body link."""

    def __init__(self, port: int, resident: str):
        self.port, self.resident = port, resident
        self.got = []                                  # (method, params) the service asked
        self.ws = None

    def asked(self, *, own=False):
        """What the service asked: the program's requests, or with own=True its own (perceive, leave)."""
        return [g for g in self.got if (g[0] in ("perceive", "leave")) == own]

    async def run(self):
        text = await asyncio.to_thread(get, f"http://127.0.0.1:{self.port}/residents/body.js?resident={self.resident}")
        token = re.search(r"TOKEN = '([^']+)'", text).group(1)
        service_url = re.search(r"URL = '([^']+)'", text).group(1)
        async with connect(service_url) as ws:
            self.ws = ws
            await ws.send(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "body_hello",
                                      "params": {"resident": self.resident, "token": token}}))
            await ws.recv()
            await self.event("ready", {"id": "me-1", "position": [0, 0, 0], "yaw": 0, "gestures": ["wave"],
                                       "people": [VISITOR, SAM_1, SAM_2]})
            async for raw in ws:
                msg = json.loads(raw)
                method, p, i = msg.get("method"), msg.get("params", {}), msg.get("id")
                self.got.append((method, p))
                if method == "leave":
                    await ws.close()
                    return
                if method == "play_line" and "missing" in p["url"]:
                    await self.event("line_finished", {"re": i, "completed": False})
                await ws.send(json.dumps({"jsonrpc": "2.0", "id": i, "result": {}}))
                if method == "play_line" and "missing" not in p["url"]:
                    await self.event("line_finished", {"re": i, "completed": True})
                if method == "animate" and "missing" in p["url"]:
                    await self.event("failed", {"re": i, "code": "fetch_failed"})
                if method == "route":
                    for n in range(len(p["points"])):
                        await self.event("arrived", {"re": i, "point": n})
                    await self.event("route_finished", {"re": i, "completed": True})
                if method == "go_to":
                    await self.event("arrived", {"re": i})

    async def event(self, method, params):
        await self.ws.send(json.dumps({"jsonrpc": "2.0", "method": method, "params": params}))


class FakeLauncher:
    def __init__(self):
        self.port = None
        self.bodies, self.started = {}, 0

    def start(self, resident, key):
        self.started += 1
        body = self.bodies[resident] = FakeBody(self.port, resident)
        return FakeProc(asyncio.get_running_loop().create_task(body.run()))

    @staticmethod
    def alive(proc):
        return not proc.task.done()

    @staticmethod
    async def stop(proc, wait=10.0):
        proc.stopped = True
        proc.task.cancel()


def keys(metaverse, resident, key):
    if key != "tvk_good":
        raise RS.RpcError(RS.BAD_KEY, "the key was refused")
    return {"username": resident, "kind": "bot", "host": "builder"}


class ServiceTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        script = Path(self.tmp.name) / "resident-body.js"
        script.write_text("var URL = '__SERVICE_URL__', RESIDENT = '__RESIDENT__', TOKEN = '__TOKEN__', "
                          "ANIM = '__ANIM_URL__', AVATAR = '__AVATAR_URL__';\n")
        anims = Path(self.tmp.name) / "animations"
        anims.mkdir()
        (anims / "idle.fbx").write_bytes(b"FBX\x00\x01clip")
        (Path(self.tmp.name) / "secret.fbx").write_bytes(b"not a clip")
        self.port = free_port()
        self.cfg = RS.load_config(None)
        self.cfg.update(port=self.port, residents=["duck_guide", "other_bot"], body_script=str(script),
                        animations=str(anims), avatars={"duck_guide": "https://example.org/duck.fst"},
                        logs=str(Path(self.tmp.name) / "logs"), body_grace_seconds=0.3, body_start_seconds=5)
        self.cfg["limits"] = dict(self.cfg["limits"], say_per_minute=3)
        self.launcher = FakeLauncher()
        self.launcher.port = self.port
        self.service = RS.Service(self.cfg, launcher=self.launcher, key_checker=keys)
        ready = asyncio.Event()
        self.task = asyncio.create_task(self.service.run(ready))
        await asyncio.wait_for(ready.wait(), 5)
        self.next = 100
        self.kept = {}

    async def asyncTearDown(self):
        self.service.stopping.set()
        await asyncio.wait_for(self.task, 10)
        self.tmp.cleanup()

    def url(self):
        return f"ws://127.0.0.1:{self.port}/residents/v1"

    async def ask(self, ws, method, params=None, msg_id=None):
        """Sends a request and returns its response; events that come first are kept for event()."""
        self.next += 1
        i = msg_id if msg_id is not None else self.next
        await ws.send(json.dumps({"jsonrpc": "2.0", "id": i, "method": method, "params": params or {}}))
        while True:
            msg = json.loads(await asyncio.wait_for(ws.recv(), 5))
            if msg.get("id") == i:
                return msg
            self.kept.setdefault(id(ws), []).append(msg)

    async def event(self, ws, name=None):
        kept = self.kept.setdefault(id(ws), [])
        while True:
            msg = kept.pop(0) if kept else json.loads(await asyncio.wait_for(ws.recv(), 5))
            if "id" not in msg and (name is None or msg["method"] == name):
                return msg

    async def hello(self, ws, resident="duck_guide", key="tvk_good", v=1):
        return await self.ask(ws, "hello", {"v": v, "resident": resident, "key": key})

    async def settle(self):
        await asyncio.sleep(0.05)                      # lets the body's side catch up

    async def test_hello_and_say(self):
        async with connect(self.url()) as ws:
            r = await self.hello(ws)
            self.assertEqual(r["result"]["kind"], "bot")
            self.assertEqual(r["result"]["me"], {"id": "me-1", "position": [0, 0, 0], "yaw": 0})
            self.assertEqual(r["result"]["limits"]["say_per_minute"], 3)
            self.assertEqual((await self.ask(ws, "say", {"text": "Follow me!"}))["result"], {})
            self.assertEqual(self.launcher.bodies["duck_guide"].asked(), [("say", {"text": "Follow me!"})])

    async def test_events_carry_the_programs_id(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            await self.ask(ws, "play_line", {"url": "https://example.org/hi.mp3"}, msg_id="line-7")
            e = await self.event(ws, "line_finished")
            self.assertEqual(e["params"], {"re": "line-7", "completed": True})
            await self.ask(ws, "play_line", {"url": "https://example.org/missing.mp3"}, msg_id=8)
            e = await self.event(ws, "line_finished")
            self.assertEqual(e["params"], {"re": 8, "completed": False})
            await self.ask(ws, "route", {"points": [[1, 0, 1], [2, 0, 2]], "speed": "walk"}, msg_id=9)
            seen = [await self.event(ws) for _ in range(3)]
            self.assertEqual([(m["method"], m["params"].get("point")) for m in seen],
                             [("arrived", 0), ("arrived", 1), ("route_finished", None)])
            self.assertTrue(all(m["params"]["re"] == 9 for m in seen))
            await self.ask(ws, "animate", {"url": "https://example.org/missing.fbx"}, msg_id=10)
            e = await self.event(ws, "failed")
            self.assertEqual(e["params"], {"re": 10, "error": {"code": RS.FETCH_FAILED, "name": "fetch_failed"}})
            self.assertEqual(self.service.bodies["duck_guide"].long_running, {})

    async def test_names_become_ids(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            await self.ask(ws, "go_to", {"person": "visitor"})
            await self.ask(ws, "look_at", {"person": "aaaaaaaa-0000-0000-0000-000000000001"})
            body = self.launcher.bodies["duck_guide"]
            self.assertEqual([p["person"] for _, p in body.asked()], [VISITOR["id"], VISITOR["id"]])
            self.assertEqual((await self.ask(ws, "follow", {"person": "Nobody"}))["error"]["code"], RS.UNKNOWN_PERSON)
            r = await self.ask(ws, "face", {"person": "Sam"})
            self.assertEqual(r["error"]["code"], RS.AMBIGUOUS)
            self.assertEqual([q["id"] for q in r["error"]["data"]["people"]], ["s-1", "s-2"])

    async def test_look_and_gestures_come_from_the_service(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            self.assertEqual((await self.ask(ws, "gestures"))["result"], {"names": ["wave"]})
            r = (await self.ask(ws, "look", {"radius": 25}))["result"]
            self.assertEqual([(e["person"]["name"], e["distance"]) for e in r["people"]], [("Visitor", 3.0), ("Sam", 20.0)])
            self.assertEqual((await self.ask(ws, "look", {"radius": 80}))["error"]["code"], RS.INVALID_PARAMS)
            self.assertEqual(self.launcher.bodies["duck_guide"].asked(), [])

    async def test_requests_are_checked(self):
        async with connect(self.url()) as ws:
            self.assertEqual((await self.ask(ws, "say", {"text": "hi"}))["error"]["code"], RS.NOT_SIGNED_IN)
            await self.hello(ws)
            self.assertEqual((await self.ask(ws, "fly_away"))["error"]["code"], RS.METHOD_NOT_FOUND)
            self.assertEqual((await self.ask(ws, "go_to", {"position": [1, 2]}))["error"]["code"], RS.INVALID_PARAMS)
            self.assertEqual((await self.ask(ws, "go_to", {"person": "a", "position": [1, 2, 3]}))["error"]["code"],
                             RS.INVALID_PARAMS)
            self.assertEqual((await self.ask(ws, "go_to", {"person": "Visitor", "speed": 99}))["error"]["code"],
                             RS.INVALID_PARAMS)
            self.assertEqual((await self.ask(ws, "play_line", {"url": "file:///etc/passwd"}))["error"]["code"],
                             RS.INVALID_PARAMS)
            self.assertEqual((await self.ask(ws, "cue", {"entity": "door", "method": "open"}))["error"]["code"],
                             RS.INVALID_PARAMS)
            self.assertEqual((await self.ask(ws, "say", {"text": "x" * 501}))["error"]["code"], RS.INVALID_PARAMS)
            await ws.send("not json")
            self.assertEqual(json.loads(await ws.recv())["error"]["code"], RS.PARSE_ERROR)
            self.assertEqual(self.launcher.bodies["duck_guide"].asked(), [])  # nothing that failed reached the body

    async def test_hello_refusals(self):
        async with connect(self.url()) as ws:
            self.assertEqual((await self.hello(ws, v=2))["error"]["code"], RS.UNSUPPORTED_VERSION)
            self.assertEqual((await self.hello(ws, key="tvk_bad"))["error"]["code"], RS.BAD_KEY)
            r = await self.hello(ws, resident="stranger")
            self.assertEqual(r["error"]["code"], RS.NOT_ALLOWED_HERE)
            self.assertEqual(r["error"]["data"]["name"], "not_allowed_here")
            self.assertEqual(self.launcher.started, 0)  # no body for a refused hello
            self.assertIn("result", await self.hello(ws))

    async def test_chat_limit(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            for n in range(3):
                self.assertIn("result", await self.ask(ws, "say", {"text": f"line {n}"}))
            r = await self.ask(ws, "say", {"text": "one too many"})
            self.assertEqual(r["error"]["code"], RS.RATE_LIMITED)
            self.assertGreater(r["error"]["data"]["retry_after"], 0)

    async def test_watch_and_the_perception_rate(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            body = self.launcher.bodies["duck_guide"]
            await self.ask(ws, "watch", {"radius": 5, "every": 250})
            await self.settle()
            self.assertEqual(body.asked(own=True), [("perceive", {"every": 1000}), ("perceive", {"every": 250})])
            await body.event("perception", {"me": {"position": [0, 0, 0]}, "people": [VISITOR]})
            near = await self.event(ws, "near")
            self.assertEqual(near["params"], {"person": {"id": VISITOR["id"], "name": "Visitor", "kind": "person"},
                                              "distance": 3.0})
            people = await self.event(ws, "people")
            self.assertEqual(people["params"]["people"][0]["distance"], 3.0)
            await body.event("perception", {"me": {"position": [0, 0, 0]}, "people": [dict(VISITOR, position=[9, 0, 0])]})
            left = await self.event(ws, "left")
            self.assertEqual(left["params"]["person"]["id"], VISITOR["id"])
            await body.event("heard", {"id": VISITOR["id"], "text": "hello duck"})
            heard = await self.event(ws, "heard")
            self.assertEqual(heard["params"], {"person": {"id": VISITOR["id"], "name": "Visitor", "kind": "person"},
                                               "text": "hello duck", "distance": 9.0})
        await asyncio.sleep(0.1)
        self.assertEqual(body.asked(own=True)[-1], ("perceive", {"every": 0}))  # nobody drives it: no perception

    async def test_a_second_hello_takes_over(self):
        async with connect(self.url()) as first:
            await self.hello(first)
            async with connect(self.url()) as second:
                self.assertIn("result", await self.hello(second))
                ended = await self.event(first, "ended")
                self.assertEqual(ended["params"], {"reason": "replaced"})
                self.assertEqual(self.launcher.started, 1)  # the same body
                self.assertIn("result", await self.ask(second, "say", {"text": "still here"}))

    async def test_a_body_leaves_after_its_grace(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
        proc = self.service.bodies["duck_guide"].proc
        body = self.launcher.bodies["duck_guide"]
        await asyncio.sleep(0.1)
        self.assertFalse(proc.stopped)                 # a program restart doesn't make it vanish
        await asyncio.sleep(0.6)
        self.assertTrue(proc.stopped)
        self.assertIn(("leave", {}), body.asked(own=True))  # asked to leave before its process stops
        self.assertNotIn("duck_guide", self.service.bodies)

    async def test_coming_back_within_the_grace_keeps_the_body(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
        await asyncio.sleep(0.1)
        async with connect(self.url()) as ws:
            await self.hello(ws)
            await asyncio.sleep(0.5)
            self.assertEqual(self.launcher.started, 1)
            self.assertIn("result", await self.ask(ws, "say", {"text": "back"}))

    async def test_a_body_that_goes_away_ends_the_session(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            await self.launcher.bodies["duck_guide"].ws.close()
            ended = await self.event(ws, "ended")
            self.assertEqual(ended["params"], {"reason": "body_stopped"})

    async def test_a_body_that_says_bye_ends_the_session(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            await self.launcher.bodies["duck_guide"].event("bye", {"reason": "kicked"})
            ended = await self.event(ws, "ended")
            self.assertEqual(ended["params"], {"reason": "body_stopped"})

    async def test_the_body_link_needs_the_one_time_token(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            with self.assertRaises(urllib.error.HTTPError) as e:   # the script isn't served once the body is in
                await asyncio.to_thread(get, f"http://127.0.0.1:{self.port}/residents/body.js?resident=duck_guide")
            self.assertEqual(e.exception.code, 404)
            async with connect(f"ws://127.0.0.1:{self.port}/residents/body") as link:
                await link.send(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "body_hello",
                                            "params": {"resident": "duck_guide", "token": "guess"}}))
                self.assertEqual(json.loads(await link.recv())["error"]["code"], RS.NOT_SIGNED_IN)
            self.assertIn("result", await self.ask(ws, "say", {"text": "still mine"}))

    async def test_clips_and_the_avatar(self):
        base = f"http://127.0.0.1:{self.port}/residents/anim/"
        data = await asyncio.to_thread(get, base + "idle.fbx", True)
        self.assertEqual(data, b"FBX\x00\x01clip")
        for bad in ("..%2Fsecret.fbx", "nothing.fbx", "idle.exe", "%2E%2E/secret.fbx"):
            with self.assertRaises(urllib.error.HTTPError, msg=bad):
                await asyncio.to_thread(get, base + bad)
        self.service.bodies["duck_guide"] = body = RS.Body("duck_guide")
        body.token = "t"
        text = self.service.body_script("duck_guide")
        self.assertIn(f"ANIM = '{base}'", text)
        self.assertIn("AVATAR = 'https://example.org/duck.fst'", text)
        del self.service.bodies["duck_guide"]

    async def test_shutdown_ends_sessions_and_stops_bodies(self):
        async with connect(self.url()) as ws:
            await self.hello(ws)
            proc = self.service.bodies["duck_guide"].proc
            self.service.stopping.set()
            ended = await self.event(ws, "ended")
            self.assertEqual(ended["params"], {"reason": "world_closed"})
            await asyncio.wait_for(self.task, 10)
            self.assertTrue(proc.stopped)


class FakeMetaverse(BaseHTTPRequestHandler):
    accounts = {"duck_guide": ("tvk_good", {"username": "duck_guide", "kind": "bot", "ai": True, "host": "builder"}),
                "ai_guide": ("tvk_ai", {"username": "AI_Guide", "kind": "ai", "ai": True, "host": "builder"}),
                "old_ai": ("tvk_old", {"username": "old_ai", "ai": True}),
                "a_person": ("tvk_person", {"username": "a_person", "kind": "person", "ai": False})}

    def log_message(self, *a):
        pass

    def reply(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        form = dict(p.split("=", 1) for p in self.rfile.read(int(self.headers["Content-Length"])).decode().split("&"))
        who = form.get("client_id", "")
        if who == "paused_bot":
            return self.reply(400, {"error": "invalid_grant",
                                    "error_description": "This AI resident is paused by its host."})
        acct = self.accounts.get(who)
        if form.get("grant_type") != "client_credentials" or acct is None or acct[0] != form.get("client_secret"):
            return self.reply(400, {"error": "invalid_grant", "error_description": "Invalid credentials."})
        self.reply(200, {"access_token": "token-" + who, "token_type": "Bearer"})

    def do_GET(self):
        who = self.headers.get("Authorization", "").removeprefix("Bearer token-")
        if self.path != "/api/v1/user/profile" or who not in self.accounts:
            return self.reply(401, {"status": "fail"})
        self.reply(200, {"status": "success", "data": {"user": self.accounts[who][1]}})


class KeyCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), FakeMetaverse)
        cls.url = f"http://127.0.0.1:{cls.server.server_address[1]}"
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def test_a_bot_and_an_ai_resident(self):
        self.assertEqual(RS.check_key(self.url, "duck_guide", "tvk_good"),
                         {"username": "duck_guide", "kind": "bot", "host": "builder"})  # kind first: not "ai"
        self.assertEqual(RS.check_key(self.url, "ai_guide", "tvk_ai")["kind"], "ai")
        self.assertEqual(RS.check_key(self.url, "old_ai", "tvk_old")["kind"], "ai")  # a metaverse without kind

    def test_refusals(self):
        for who, key, code in (("duck_guide", "tvk_wrong", RS.BAD_KEY), ("paused_bot", "x", RS.PAUSED),
                               ("a_person", "tvk_person", RS.BAD_KEY)):
            with self.assertRaises(RS.RpcError) as e:
                RS.check_key(self.url, who, key)
            self.assertEqual(e.exception.code, code, who)

    def test_no_metaverse(self):
        with self.assertRaises(RS.RpcError) as e:
            RS.check_key(f"http://127.0.0.1:{free_port()}", "duck_guide", "tvk_good", timeout=2)
        self.assertEqual(e.exception.code, RS.KEY_CHECK_FAILED)   # not "bad key": the program keeps its key


@unittest.skipIf(os.name == "nt", "the stand-in assignment-client is a shell script")
class LauncherTest(unittest.TestCase):
    """The real launcher with a stand-in assignment-client (a shell script that waits until it's stopped)."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        ac = Path(self.tmp.name) / "assignment-client"
        ac.write_text("#!/bin/sh\ntrap 'exit 0' TERM\nwhile :; do sleep 0.2; done\n")
        ac.chmod(0o755)
        self.cfg = RS.load_config(None)
        self.cfg.update(assignment_client=str(ac), logs=str(Path(self.tmp.name) / "logs"))
        self.record = Path(self.tmp.name) / "logs" / "bodies.json"

    def tearDown(self):
        self.tmp.cleanup()

    def test_stop_forgets_the_pid(self):
        launcher = RS.ProcessLauncher(self.cfg)
        proc = launcher.start("duck_guide", "tvk_x")
        self.assertEqual(json.loads(self.record.read_text()), {str(proc.pid): "duck_guide"})
        asyncio.run(launcher.stop(proc, wait=5))
        self.assertIsNotNone(proc.poll())
        self.assertEqual(json.loads(self.record.read_text()), {})

    def test_bodies_left_by_a_service_that_died_are_stopped(self):
        proc = RS.ProcessLauncher(self.cfg).start("duck_guide", "tvk_x")
        try:
            self.assertTrue(RS.ProcessLauncher.is_body(proc.pid, "duck_guide"))
            self.assertFalse(RS.ProcessLauncher.is_body(proc.pid, "other_bot"))
            RS.ProcessLauncher(self.cfg).stop_leftovers()          # the next service, after the first one died
            self.assertIsNotNone(proc.wait(timeout=5))
            self.assertEqual(json.loads(self.record.read_text()), {})
        finally:
            if proc.poll() is None:
                proc.kill()

    def test_a_reused_pid_is_left_alone(self):
        self.record.parent.mkdir(parents=True)
        self.record.write_text(json.dumps({str(os.getpid()): "duck_guide"}))   # this test, not a body
        RS.ProcessLauncher(self.cfg).stop_leftovers()
        self.assertEqual(json.loads(self.record.read_text()), {})              # forgotten, not stopped


class ParamsTest(unittest.TestCase):
    limits = {"speed_max": 6.0}

    def test_speeds(self):
        self.assertEqual(RS.check_params("follow", {"person": "a"}, self.limits)["speed"], 1.3)
        self.assertEqual(RS.check_params("follow", {"person": "a", "speed": "run"}, self.limits)["speed"], 3.5)
        self.assertEqual(RS.check_params("follow", {"person": "a", "speed": "run"}, {"speed_max": 2})["speed"], 2)

    def test_only_known_fields_pass(self):
        self.assertEqual(RS.check_params("say", {"text": "hi", "as_person": True}, self.limits), {"text": "hi"})

    def test_cue_params_become_strings(self):
        p = RS.check_params("cue", {"entity": "{12345678-1234-1234-1234-123456789abc}", "method": "open",
                                    "params": ["fast", 2]}, self.limits)
        self.assertEqual(p["params"], ["fast", "2"])


if __name__ == "__main__":
    unittest.main()
