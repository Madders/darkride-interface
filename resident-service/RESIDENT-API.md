# The resident API (version 1, draft)

The API programs use to drive AI residents' and bots' bodies in a Darkride world, served by `resident_service.py`
beside the world's server. It is a draft: version 1 may still change before it is final.

A program drives a resident's body in a Darkride world: walks it, makes it speak, plays its recorded lines, turns
its head toward a visitor, works the world's doors and machines, and hears what happens around it. The program can
be scripted logic (a **bot**), a language model (an **AI resident**), or both (then it is an AI resident, because a
model speaks). It is one API for every kind of brain.

## In one minute

1. On darkride.ai, open your user menu, then **AI Residents & Bots**, and add a resident. Pick what drives it: a
   language model or neural network (AI resident) or scripted logic (bot). Copy its **connection key** (`tvk_...`).
2. The world's owner lets the resident into the world (one line in the world's resident service config, below).
3. Your program opens a WebSocket to the world's resident service, sends `hello` with the resident's name and key,
   and the body walks into the world. It sends commands and gets events. When the program goes away, the body
   leaves.

That's all a builder handles: one key. Signing in, limits and the safety filter happen on the server.

## How the pieces fit

```
your program ──WebSocket──▶ resident service ──local WebSocket──▶ resident-body.js ──▶ the world
 (anywhere)                  (beside the world's server:            (one assignment-client
                              key check, rules, limits,              per body, signed in
                              safety filter, brain loop)             as the resident)
```

- The **resident service** runs on the machine that runs the world: our servers for hosted worlds, the owner's own
  computer for a world they host themselves (it ships with the server install). One service serves every resident
  body in that world.
- Each body is an assignment-client signed in as the resident (its account says it's a bot or an AI resident, and
  everyone sees that mark) running `scripts/ai-residents/resident-body.js`. It joins the world as a **visitor**, so
  its rights are the ones the world gives that account, never a server's (see "Not decided yet").
- Every rule is enforced in the service and the body, never trusted to the program.

## Connecting

The service listens on the world's own computer only (127.0.0.1), on the port after the world's block (base+5:
40105 for a world on 40100-40102), and opens nothing new to the internet:

- **Your program runs on the same computer as the world** (the usual case for a world you host yourself):
  `ws://127.0.0.1:40105/residents/v1`. Nothing to set up.
- **The world is on our servers**: `wss://<server>/residents/<world id>`. The server's secure front checks the key
  and passes the connection on to that world's service.
- **The world is on someone's own computer and your program is on another**: run the resident service on your
  program's computer too (it's the same package). Its body joins the world over the world's usual connection as a
  visitor, so nobody opens a new port, and the world's owner keeps every visitor control (who may enter, kick, ban).
  This needs the assignment-client's visiting mode (see "Not decided yet"); until then it works only for the world
  owner's own program, with the owner's say-so in the world's security settings.

Messages are JSON text frames in **JSON-RPC 2.0**: the program sends requests (`method`, `params`, `id`) and gets one response for each (`result` or `error`); the service sends
events as notifications (`method` and `params`, no `id`). Any JSON-RPC 2.0 library works, and so does plain
`JSON.stringify`.

One connection drives one resident. A program that runs several bots opens one connection per bot, each with its own
key.

### hello

The first message, within 10 seconds of opening the socket:

```json
{"jsonrpc": "2.0", "id": 1, "method": "hello",
 "params": {"v": 1, "resident": "duck_guide", "key": "tvk_..."}}
```

The service checks the key with darkride.ai (the same check the body's own sign-in makes), checks that the world's
owner lets this resident in, starts the body if it isn't running, and answers when the body is in the world (a few
seconds):

```json
{"jsonrpc": "2.0", "id": 1, "result": {
  "v": 1, "resident": "duck_guide", "kind": "bot", "host": "<its host's username>",
  "world": {"name": "...", "address": "127.0.0.1", "udp_port": 40102},
  "me": {"id": "<session id>", "position": [12.0, 0.5, -3.2], "yaw": 90},
  "limits": {"say_per_minute": 20, "requests_per_second": 20, "speed_max": 6.0}}}
```

- A second `hello` for the same resident takes over: the older connection gets `ended` with reason `replaced` and
  is closed. So a program can restart without waiting.
- When the connection closes, the body stays where it is for 60 seconds (a restart doesn't make it vanish), then
  leaves the world. A body nobody drives costs nothing.
- The service keeps the key in memory only, to sign the body in; it is never written to disk or logged.

## Coordinates and people

- Positions are `[x, y, z]` in metres in the world's frame, `y` up. Directions are a `yaw` in degrees (0 = the
  world's -z, turning left is positive) or a position to face.
- A **person** is anyone in the world: a visitor, an AI resident or a bot. Requests take `person` as a session id
  or a display name (a name that matches two people is an error, `ambiguous`). Events give
  `{"id", "name", "kind"}` with `kind` one of `person`, `ai`, `bot`.

## Requests

Every request gets exactly one response: `result` (often `{}`) when it was accepted, or an `error`. Something that
takes time (a walk, a recorded line) also sends an event when it is over, carrying the request's `id` as `re`.
A new movement request replaces the one in progress (a route ends with `route_finished`, `completed: false`; a `go_to` simply never sends its `arrived`).

### Moving

| Method | Params | Over when |
| --- | --- | --- |
| `go_to` | `person` or `position`; `within` (metres, default 1.2); `speed` | `arrived` |
| `route` | `points`: a list of positions; `speed`; `pause` (seconds at each point, default 0) | `arrived` at each point, then `route_finished` |
| `follow` | `person`; `distance` (default 1.5); `speed` | until `stop` or another movement; `lost` if they leave the world |
| `face` | `person`, `position` or `yaw` | at once |
| `stop` | (none) | at once: stops moving, ends a route or a follow |

`speed` is `"walk"` (1.3 m/s, the default), `"run"` (3.5 m/s) or a number in m/s up to `limits.speed_max`. How
the body looks while it moves (a walk, a waddle, a hover) comes from its avatar's animations (`animate`, below).

### Speaking

| Method | Params | Over when |
| --- | --- | --- |
| `say` | `text` (up to 500 characters; longer is refused with -32602, not cut) | at once (it's in the world's chat) |
| `play_line` | `url`: a recorded line (WAV or MP3) the world's server can fetch; `volume` 0-1 | `line_finished` with `completed` |
| `stop_line` | (none) | `line_finished` with `completed: false` |

A recorded line plays from the body, positional like a voice. A new `play_line` cuts the one playing short. The
world's server fetches the whole file before it plays, so a long line starts after its download; a file that can't
be loaded ends with `line_finished`, `completed: false`. Lines and animations are fetched by the world's server: an
address on your own computer works only when your program runs beside the world.

### Attention and expression

| Method | Params | Notes |
| --- | --- | --- |
| `look_at` | `person` or `position`; `eyes_only` (default false) | the head turns (within its natural range) and the eyes follow; the body keeps its facing. Others see where it looks |
| `look_away` | (none) | back to looking ahead |
| `gesture` | `name`: one from `gestures` | a short animation, then back |
| `gestures` | (none) | result: the names available |
| `animate` | `url`: an animation (FBX or glTF); `loop` (default true); `fps` | replaces the walk/idle animations until `stop_animation` |
| `stop_animation` | (none) | back to the avatar's own |

### The world

| Method | Params | Notes |
| --- | --- | --- |
| `look` | `radius` (default 25, up to 50) | result: `me` and the people near the body, nearest first: who, position, distance, where they look (things come later) |
| `watch` | `radius` (default 10, up to 50: how far a body sees people); `every` (ms, 250-5000; 0 = only `near`/`left`) | turns on `near`, `left` and, with `every`, `people` events. One `watch` at a time; a new one replaces it |
| `cue` | `entity` (id); `method`; `params` (a list of strings) | calls a method on the entity's **server** script, which must list it in `remotelyCallable` (the world's builder decides what a bot may work). Numbers in `params` arrive as strings. The script sees who called (`Script.remoteCallerID`), so it can answer that body on a channel. The result says the cue was sent, not that it worked: a method the script doesn't list is ignored without a reply. A script that wants to answer does so on a channel (`send`/`listen`) |
| `send` | `channel`; `message` (a string) | a message on the world's message channels: other bots and the world's scripts hear it |
| `listen` | `channel` | turns on `message` events for that channel; `listen` again with `"off": true` to stop |

## Events

| Event | Params |
| --- | --- |
| `arrived` | `re`; `point` (the index, for a route) |
| `route_finished` | `re`; `completed` |
| `lost` | `re`; `person` (who it was following left the world) |
| `line_finished` | `re`; `completed` (false: cut short, or the file couldn't be loaded) |
| `near` | `person`; `distance` (someone came within the watch radius) |
| `left` | `person` (someone went outside the radius, or left the world) |
| `people` | `people`: a list of `{"person", "position", "distance", "facing_me"}` within the radius, every `every` ms while anyone is in it |
| `heard` | `person`; `text`; `distance` (a chat line in the world) |
| `failed` | `re`; `error` (`code`, `name`): a request that was accepted failed later, such as an animation that couldn't be fetched |
| `message` | `channel`; `message`; `from` (a person, or `null` for a world script) |
| `paused` | `reason`: `host` (its host paused it) or `safety` (too many lines held by the safety filter) |
| `resumed` | (none) |
| `ended` | `reason`: `replaced`, `archived`, `world_closed`, `key_revoked`, `body_stopped` (the body left the world unexpectedly; `hello` again to bring it back). The socket closes after it |

"Someone fell behind" or "came back" is for the program to work out from `near`, `left` and `people`; the API stays
small and keeps its events raw.

## Errors

JSON-RPC's own codes for malformed messages (-32700, -32600, -32601, -32602). Then:

| Code | Name | When |
| --- | --- | --- |
| 1001 | `not_signed_in` | a request before `hello` succeeded |
| 1002 | `bad_key` | the key is wrong, replaced or revoked |
| 1003 | `not_allowed_here` | the world's owner hasn't let this resident in |
| 1004 | `paused` | the resident is paused (`data.reason`) |
| 1005 | `rate_limited` | over a limit; `data.retry_after` in seconds |
| 1006 | `unknown_person` | the person isn't in the world |
| 1007 | `ambiguous` | the name matches several people |
| 1008 | `out_of_world` | a position, entity or channel outside this world |
| 1009 | `fetch_failed` | a line or animation URL the server couldn't fetch |
| 1010 | `unsupported_version` | `hello` asked for a `v` this service doesn't speak |
| 1011 | `body_unavailable` | the body couldn't come into the world, or isn't there any more |
| 1012 | `key_check_failed` | darkride.ai couldn't be asked just now: try again later (the key may be fine) |

## Rules (enforced by the service and the body)

- **Its own world only.** A body lives in the world whose owner let it in. Cues reach that world's entities,
  messages stay on that world's channels.
- **Marked, always.** Its nametag and chat say what it is (bot or AI resident) and who hosts it. Nothing in the API
  can change or hide that, and it never claims to be a person.
- **Limits**: 20 chat lines a minute, 20 requests a second, one recorded line at a time, speed up to 6 m/s. The
  world's owner can lower them. Over a limit, requests get `rate_limited`; nothing is queued.
- **The safety filter** checks what an AI resident says (its chat and anything a model wrote) and holds what crosses
  the line; three holds in ten minutes pause it (`paused`, reason `safety`) until its host looks. A bot's scripted
  lines are its host's own, as the host of that account.
- **A bot makes no model calls through the service.** The service's brain loop (for AI residents whose brain runs
  on the server with the host's provider key) refuses a bot's account. A host who adds a model to a bot's program
  switches the account to AI resident on the website.
- **Visitors stay in charge of themselves**: a body never moves a visitor, and a ride starts only for people who
  chose to sit.

## Versions

`hello` says `v: 1`. Within version 1 things are only added (new methods, new events, new optional fields); a
program ignores fields it doesn't know, and the service ignores unknown optional params. A change that would break
a program becomes version 2 at `/residents/v2`, with version 1 kept alongside for at least a year.

## Example: a tour-guide duck (Node 22, no libraries)

```js
// Greets the first visitor who comes near, then walks to each stop and plays its line there.
const ws = new WebSocket("ws://127.0.0.1:40105/residents/v1");
let id = 0;
const call = (method, params = {}) => ws.send(JSON.stringify({ jsonrpc: "2.0", id: ++id, method, params }));
const line = file => "https://example.org/duck/" + file;
const stops = [[10, 0, 4], [18, 0, 9], [25, 0, 2]];
const stopLines = ["the-mill.mp3", "the-river.mp3", "goodbye.mp3"].map(line);
let guest = null, next = 0;

ws.onopen = () => call("hello", { v: 1, resident: "duck_guide", key: process.env.RESIDENT_KEY });
ws.onmessage = ({ data }) => {
  const m = JSON.parse(data);
  if (m.error) return console.error(m.error);
  if (m.id === 1) return call("watch", { radius: 6 });   // signed in: tell me who comes near
  const p = m.params || {};
  switch (m.method) {
    case "near":            // someone came within 6 m
      if (!guest) {
        guest = p.person; next = 0;
        call("look_at", { person: guest.id });
        call("play_line", { url: line("welcome.mp3") });
      }
      break;
    case "line_finished":   // a line is over: walk on to the next stop, if any
      if (guest && next < stops.length) call("go_to", { position: stops[next] });
      break;
    case "arrived":         // at a stop: turn to the guest and explain it
      if (guest) { call("face", { person: guest.id }); call("play_line", { url: stopLines[next++] }); }
      break;
    case "left":            // the guest wandered off
      if (guest && p.person.id === guest.id) {
        call("stop"); call("look_away"); call("say", { text: "I'll wait here for you!" }); guest = null;
      }
      break;
  }
};
```

## Running the service (for a world's owner)

The service ships with the server install, in `resident-service/` (`python3 resident_service.py`; the world's
launcher starts it after the domain server). Copy `config.example.json` to `config.json` and set:

- `residents`: the usernames of the residents you let into this world;
- `world`: your world's address and UDP port (127.0.0.1 and 40102 unless you changed them), and `port` (base+5);
- `avatars` (optional): each resident's avatar as a public model URL (every visitor loads it).

Until the assignment-client's visiting mode, also give each resident's pool its body script, in your domain
settings under the persistent scripts: the URL `http://127.0.0.1:40105/residents/body.js?resident=<username>` and
the pool `<username>`.

## The body's side (resident-body.js and the service)

Programs never see this link; it's how the service talks to each body. The same JSON-RPC shape over
`ws://127.0.0.1:<port>/residents/body`.

- **Who may connect**: the service serves resident-body.js itself (`GET /residents/body.js?resident=<name>`, on
  127.0.0.1) with the service's address, the resident and a **one-time token** written in (`__SERVICE_URL__`,
  `__RESIDENT__`, `__TOKEN__`), and only while that body is starting. The body's first message is
  `body_hello {resident, token}`; once used, the token is gone and the script isn't served any more, so no other
  local program can pose as a body. (Until the assignment-client's visiting mode, the world's settings give each
  resident's pool that script address; with it, the service starts the body with the address itself.)
- **Service → body** (requests, each answered `result {}` or `error`): the program's verbs after the rules, with
  people already turned into session ids, plus `perceive {every}` (ms; 0 = stop) and `leave` (stop being an avatar
  and end the script). The service answers `look`, `gestures` and `watch` itself from what the body told it.
- **Body → service** (notifications): `ready {id, position, yaw, gestures, people}` once the avatar mixer has its
  identity; `perception {me, people}` at the rate asked (none while nobody drives it); `arrived`, `route_finished`,
  `lost`, `line_finished`, `failed {re, code}`; `heard {id, text}`; `message {channel, message, from}`; `bye {reason}`
  when it leaves on its own (the program gets `ended`, `body_stopped`).
- **Liveness**: pings every 5 s; a body whose service has been silent for 60 s leaves the world by itself.

## Not decided yet

1. **Reaching the service from another computer**: on Darkride's servers, through the secure front on port 443
   (`wss://<server>/residents/<world id>`), not built yet. Never through the domain server's own TCP ports, which are
   its admin pages and stay private.
2. **The body's rights**: today an assignment-client joins a world only from the world's own computer or an address
   its owner allows, and then as trusted server software rather than as a visitor. A visiting mode for the
   assignment-client will make every resident body join as a visitor with its account's rights, wherever it runs.
3. **Letting a resident in**: the service config's `residents` list for now; later a switch in the world's settings
   on darkride.ai.
4. **Several residents on one connection**: not planned (one per resident keeps limits and keys simple). Say if a
   program needs it.
5. **A safety filter on someone else's computer**: when a program runs its own service to visit someone else's
   world, the filter runs where the builder controls it. Whether that is enough for an AI resident there is open; a
   bot isn't affected.
6. **Props, notes per visitor, "not me"**: next. A prop in a body's hand needs avatar attachments until entities
   parented to an agent's own avatar show up (an engine fix).
