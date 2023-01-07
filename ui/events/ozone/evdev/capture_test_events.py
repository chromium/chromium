#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code generator for logs based tests."""

import argparse
import evdev
import os
import sys

# Fix keys with multiple names.
try:
  import explain_proc_bus_input_devices
  KEY_NAMES = explain_proc_bus_input_devices.KEY_NAMES
except:
  KEY_NAMES = evdev.ecodes.bytype[evdev.ecodes.EV_KEY].copy()


def dump_events(out, dev):
  out.write('const struct input_event mock_kernel_queue[] = {\n')

  try:
    for event in dev.read_loop():
      if event.type == evdev.ecodes.EV_KEY:
        CODE_NAMES = KEY_NAMES
      else:
        CODE_NAMES = evdev.ecodes.bytype[event.type]

      out.write(
          '    {{%(sec)d, %(usec)d}, %(type)s, %(code)s, %(value)d},\n' % {
              'sec': event.sec,
              'usec': event.usec,
              'type': evdev.ecodes.EV[event.type],
              'code': CODE_NAMES[event.code],
              'value': event.value
          })
  except KeyboardInterrupt as e:
    pass

  out.write('};\n')


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('device')
  args = parser.parse_args(argv)

  dev = evdev.InputDevice(args.device)
  out = sys.stdout

  dump_events(out, dev)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
