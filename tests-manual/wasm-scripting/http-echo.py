#
#  http-echo.py
#  tests-manual/wasm-scripting
#
#  The web server of server-check.sh, on 127.0.0.1 only: GET serves the files of the checkout (the agent's script, the
#  edit filter, a sound), and a POST to /echo answers {"ok": true, "length": <bytes received>}, as a bridge would
#  answer an agent. It logs nothing of what it is sent.
#    python3 -I http-echo.py <port> <directory>
#
#  Distributed under the Apache License, Version 2.0.
#
import functools
import http.server
import json
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        self.rfile.read(length)
        if self.path != "/echo":
            self.send_error(404)
            return
        body = json.dumps({"ok": True, "length": length}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        pass


port, directory = int(sys.argv[1]), sys.argv[2]
server = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=directory))
server.serve_forever()
