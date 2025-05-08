#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import subprocess
import sys

import run_annotator_on_chrome_java


def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    parser = argparse.ArgumentParser()
    java_files, javac_cmd = run_annotator_on_chrome_java.prep_errorprone_run(
        False, parser)
    logging.info('Running NullAway on %d @NullMarked files in chrome_java',
                 len(java_files))
    sys.exit(subprocess.run(javac_cmd).returncode)


if __name__ == '__main__':
    main()
