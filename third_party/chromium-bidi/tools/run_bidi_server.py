#!/usr/bin/env python3

#  Copyright 2026 Google LLC.
#  Copyright (c) Microsoft Corporation.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from __future__ import annotations

import argparse
import datetime
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
from typing import IO

from test_runner_utils import get_default_chromedriver_bin, get_repo_root


def resolve_binary_path(path: str | None) -> str | None:
    """Resolves a binary path relative to the current working directory.

    If the path is already absolute, returns it as-is.
    If it has a directory component or exists, returns an absolute path.
    Otherwise checks shutil.which (e.g. for bare binary names like 'chromedriver').
    """
    if not path:
        return None
    if os.path.isabs(path):
        return path
    if os.path.exists(path) or os.path.dirname(path):
        return os.path.abspath(path)
    which_path = shutil.which(path)
    if which_path:
        return which_path
    return os.path.abspath(path)


def get_log_file_path(suffix: str) -> str:
    """Returns an absolute path for a log file in the logs directory."""
    dir_path = os.environ.get("LOG_DIR", os.path.join(get_repo_root(), "logs"))
    os.makedirs(dir_path, exist_ok=True)
    if "LOG_FILE" in os.environ:
        return os.path.abspath(os.environ["LOG_FILE"])

    timestamp = (
        datetime.datetime.now(datetime.timezone.utc).isoformat().replace(":", "-")
    )
    return os.path.abspath(os.path.join(dir_path, f"{timestamp}.{suffix}.log"))


class BiDiServerProcess:
    """Manages the lifecycle of a ChromeDriver process."""

    def __init__(
        self,
        gen_dir: str | None = None,
        node_py: str | None = None,
        port: str | int = 8080,
        browser_bin: str | None = None,
        chromedriver_bin: str | None = None,
        verbose: bool = True,
        log_file: str | None = None,
        extra_args: list[str] | None = None,
    ):
        self.repo_root = get_repo_root()
        self.gen_dir = (
            os.path.abspath(gen_dir)
            if gen_dir
            else os.path.join(self.repo_root, "out", "Default", "gen")
        )
        self.node_py = os.path.abspath(node_py) if node_py else None
        self.port = str(port)
        self.browser_bin = resolve_binary_path(
            browser_bin or os.environ.get("BROWSER_BIN")
        )
        self.chromedriver_bin = resolve_binary_path(
            chromedriver_bin or get_default_chromedriver_bin()
        )
        self.verbose = verbose
        self.log_file = log_file
        self.extra_args = extra_args or []

        self.process: subprocess.Popen | None = None
        self._ready_event = threading.Event()
        self._stop_event = threading.Event()
        self._threads: list[threading.Thread] = []
        self._log_handle: IO | None = None
        self.server_logs: list[str] = []

    def start(self) -> BiDiServerProcess:
        env = os.environ.copy()
        env["PORT"] = self.port
        if self.browser_bin:
            env["BROWSER_BIN"] = self.browser_bin
        if self.chromedriver_bin:
            env["CHROMEDRIVER_BIN"] = self.chromedriver_bin
            env["CHROMEDRIVER"] = "true"

        chromedriver_path = self.chromedriver_bin or resolve_binary_path("chromedriver")
        mapper_path = os.path.abspath(
            os.path.join(
                self.gen_dir,
                "third_party",
                "chromium-bidi",
                "src",
                "mapperTab.js",
            )
        )
        if not os.path.exists(mapper_path):
            mapper_path = os.path.abspath(
                os.path.join(self.gen_dir, "src", "mapperTab.js")
            )

        cmd = [
            chromedriver_path,
            f"--port={self.port}",
            f"--bidi-mapper-path={mapper_path}",
            "--readable-timestamp",
        ]
        if self.verbose:
            cmd.append("--verbose")
        cmd.extend(self.extra_args)

        if self.log_file:
            self._log_handle = open(self.log_file, "a", encoding="utf-8")

        self.process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            cwd=self.repo_root,
            text=True,
            bufsize=1,
        )

        for pipe_name, stream in [
            ("stdout", self.process.stdout),
            ("stderr", self.process.stderr),
        ]:
            t = threading.Thread(
                target=self._reader_thread,
                args=(pipe_name, stream),
                daemon=True,
            )
            t.start()
            self._threads.append(t)

        return self

    def _reader_thread(self, name: str, stream: IO | None):
        if stream is None:
            return
        pattern = re.compile(r".*ChromeDriver was started successfully")
        try:
            for line in iter(stream.readline, ""):
                self.server_logs.append(line)
                if pattern.search(line):
                    self._ready_event.set()

                if self._log_handle:
                    self._log_handle.write(line)
                    self._log_handle.flush()

                if self.verbose:
                    out = sys.stdout if name == "stdout" else sys.stderr
                    out.write(line)
                    out.flush()
        except Exception:
            pass

    def wait_until_ready(self, timeout: float = 10.0) -> bool:
        ready = self._ready_event.wait(timeout=timeout)
        if not ready and self.process and self.process.poll() is not None:
            raise RuntimeError(
                f"Server process terminated prematurely with return code {self.process.returncode}"
            )
        return ready

    def stop(self, timeout: float = 5.0):
        self._stop_event.set()
        if self.process:
            try:
                self.process.terminate()
                self.process.wait(timeout=timeout)
            except (subprocess.TimeoutExpired, OSError):
                self.process.kill()
                self.process.wait()
            self.process = None

        if self._log_handle:
            try:
                self._log_handle.close()
            except Exception:
                pass
            self._log_handle = None


def main():
    parser = argparse.ArgumentParser(description="Runs the WebDriver BiDi server")
    parser.add_argument("--gen-dir", help="Path to gen directory")
    parser.add_argument("--node-py", help="Path to node.py")
    parser.add_argument("--port", default=os.environ.get("PORT", "8080"))
    parser.add_argument("--browser-bin", default=os.environ.get("BROWSER_BIN"))
    parser.add_argument("--chromedriver-bin", default=get_default_chromedriver_bin())
    parser.add_argument("--verbose", action="store_true", default=True)
    parser.add_argument("--log-file", help="Path to server log file")
    args, unknown = parser.parse_known_args()

    log_path = args.log_file or get_log_file_path("server")
    print(f"(run_bidi_server.py) Logging to {log_path}")

    server = BiDiServerProcess(
        gen_dir=args.gen_dir,
        node_py=args.node_py,
        port=args.port,
        browser_bin=args.browser_bin,
        chromedriver_bin=args.chromedriver_bin,
        verbose=args.verbose,
        log_file=log_path,
        extra_args=unknown,
    )

    def handle_signal(sig, frame):
        server.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    server.start()
    try:
        if not server.wait_until_ready(timeout=10.0):
            print(
                "(run_bidi_server.py) Timeout waiting for server to be ready",
                file=sys.stderr,
            )
            server.stop()
            sys.exit(1)
        print(f"(run_bidi_server.py) BiDi server running on port {args.port}...")
        # Keep running until killed
        server.process.wait()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()


if __name__ == "__main__":
    main()
