# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A demangler relying on the in-tree tool llvm-cxxfilt."""

import os
import pathlib
import subprocess


class Demangler:
    """Demangles line by line.

    For simplicity, ignores stderr and can block forever if the external
    command does not return.
    """

    def __init__(self):
        src_path = pathlib.Path(__file__).resolve().parents[4]
        self.binary_path = str(
            src_path /
            'third_party/llvm-build/Release+Asserts/bin/llvm-cxxfilt')
        self.process = None

    def start(self):
        if not os.path.exists(self.binary_path):
            raise FileNotFoundError(f'Not found: {self.binary_path}')
        # TODO(crbug.com/473768497): Explore async demangling.
        self.process = subprocess.Popen([self.binary_path],
                                        stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL,
                                        text=True,
                                        bufsize=1)

    def demangle(self, mangled_name):
        if not self.process:
            raise RuntimeError('Process not started')

        try:
            self.process.stdin.write(mangled_name + '\n')
            self.process.stdin.flush()

            result = self.process.stdout.readline()
            return result.strip()
        except (BrokenPipeError, IOError):
            raise RuntimeError('Error: Process closed unexpectedly')

    def stop(self):
        if self.process:
            self.process.stdin.close()
            self.process.stdout.close()
            self.process.wait()
            self.process = None

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
