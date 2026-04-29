#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys


def main():
    # The first argument is the path to the real glib-compile-schemas binary.
    if len(sys.argv) < 2:
        return 1

    binary = sys.argv[1]
    args = sys.argv[2:]

    process = subprocess.Popen([binary] + args,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               text=True,
                               encoding='utf-8')
    stdout, stderr = process.communicate()

    # Filter out deprecation warnings that we don't care about in tests.
    # Warning: Schema “org.gnome.system.locale” has path “/system/locale/”.
    # Paths starting with “/apps/”, “/desktop/” or “/system/” are deprecated.
    filtered_stderr = []
    for line in stderr.splitlines():
        if "Warning: Schema" in line and "are deprecated." in line:
            continue
        filtered_stderr.append(line)

    sys.stdout.write(stdout)
    if filtered_stderr:
        sys.stderr.write("\n".join(filtered_stderr) + "\n")

    return process.returncode


if __name__ == '__main__':
    sys.exit(main())
