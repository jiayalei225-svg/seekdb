#!/usr/bin/env python3
"""Deterministic Mock AI Service used by the ai_function mysqltests.

The public tests point their endpoints at http://0.0.0.0:8000.  This server
answers the OpenAI compatible, SiliconFlow rerank and native Ollama paths with
stable payloads; it never calls a real model.
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

LOG_PATH = "/tmp/mock_ai_service_log.jsonl"


def input_count(payload):
    value = payload.get("input", "")
    return len(value) if isinstance(value, list) else 1


def requested_dimensions(payload):
    value = payload.get("dimensions")
    if value is None and isinstance(payload.get("parameters"), dict):
        value = payload["parameters"].get("dimension")
    try:
        dimensions = int(value)
    except (TypeError, ValueError):
        dimensions = 768
    return dimensions if dimensions > 0 else 768


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send_json(self, payload, status=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        try:
            request = json.loads(raw or b"{}")
        except Exception:
            request = {}
        path = self.path.split("?", 1)[0]

        if path.endswith("/chat/completions"):
            response = {"choices": [{"message": {"content": "mock: complete response"}}]}
        elif path.endswith("/embeddings"):
            count = input_count(request)
            dimensions = requested_dimensions(request)
            response = {"data": [{"embedding": [0.1] * dimensions} for _ in range(count)]}
        elif path.endswith("/rerank"):
            documents = request.get("documents") or []
            response = {
                "results": [
                    {"index": i, "relevance_score": 0.98 - 0.01 * i}
                    for i in range(len(documents))
                ]
            }
        elif path.endswith("/api/generate"):
            response = {"response": "mock: ollama generate response"}
        elif path.endswith("/api/embed"):
            count = input_count(request)
            response = {"embeddings": [[0.1] * 768 for _ in range(count)]}
        else:
            self.send_json({"error": "not found"}, 404)
            return

        try:
            with open(LOG_PATH, "a", encoding="utf-8") as log:
                log.write(json.dumps({"path": path, "request": request,
                                      "response": response}) + "\n")
        except OSError:
            pass
        self.send_json(response)


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    server.serve_forever()


if __name__ == "__main__":
    main()
