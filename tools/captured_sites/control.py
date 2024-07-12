#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs captured sites framework recording and tests."""

import sys
import signal

import captured_sites_commands


def _handle_signal(sig, _):
  """Handles received signals to make sure spawned test process are killed.

  sig (int): An integer representing the received signal, for example SIGTERM.
  """

  # Don't do any cleanup here, instead, leave it to the finally blocks.
  # Assumption is based on https://docs.python.org/3/library/sys.html#sys.exit:
  # cleanup actions specified by finally clauses of try statements are honored.

  # https://tldp.org/LDP/abs/html/exitcodes.html:
  # Exit code 128+n -> Fatal error signal "n".
  print('Signal to quit received, waiting for potential WPR write to complete')
  time.sleep(1)
  sys.exit(128 + sig)


def main():
  for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, _handle_signal)

  command = captured_sites_commands.initiate_and_build_command()
  command.launch()


if __name__ == '__main__':
  sys.exit(main())