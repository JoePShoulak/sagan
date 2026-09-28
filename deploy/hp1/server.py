#!/usr/bin/env python3
"""Serve the generated Sagan documentation from an unprivileged HP1 account."""

from __future__ import annotations

import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class DocumentationHandler(SimpleHTTPRequestHandler):
    server_version = "SaganDocs"
    sys_version = ""

    def end_headers(self) -> None:
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "strict-origin-when-cross-origin")
        self.send_header("X-Frame-Options", "SAMEORIGIN")
        super().end_headers()

    def do_GET(self) -> None:
        if self.path == "/healthz":
            body = b'{"ok":true,"service":"sagan-docs","host":"hp1"}\n'
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--port", type=int, default=8781)
    args = parser.parse_args()

    root = args.root.resolve(strict=True)
    handler = lambda *handler_args, **kwargs: DocumentationHandler(  # noqa: E731
        *handler_args, directory=str(root), **kwargs
    )
    server = ThreadingHTTPServer(("0.0.0.0", args.port), handler)
    server.serve_forever()


if __name__ == "__main__":
    main()
