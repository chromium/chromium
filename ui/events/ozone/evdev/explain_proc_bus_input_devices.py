#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Explain device capabilities from /proc/bus/input/devices.

This tool processes the contents of /proc/bus/input/devices, expanding bitfields
of event codes into lists of names for those events.
"""

import argparse
import evdev
import sys


def parse_bitfield(bitfield, word_bits):
  """Parse a serialized bitfield from /proc/bus/input/devices.

  Returns a list of bits in the set.

  Example: parse_bitfield('10 3', 64) == [0, 1, 68]
  """
  groups = bitfield.split(' ')
  group_count = len(groups)
  result_bits = []

  for group_index in xrange(group_count):
    group_val = int(groups[group_count - 1 - group_index], 16)

    for group_bit in xrange(word_bits):
      if group_val & (1 << group_bit):
        result_bits.append(group_index * word_bits + group_bit)

  return result_bits


PROP_NAMES = {
    0x0: 'INPUT_PROP_POINTER',
    0x1: 'INPUT_PROP_DIRECT',
    0x2: 'INPUT_PROP_BUTTONPAD',
    0x3: 'INPUT_PROP_SEMI_MT',
    0x4: 'INPUT_PROP_TOPBUTTONPAD',
}


KEY_NAMES = evdev.ecodes.bytype[evdev.ecodes.EV_KEY].copy()

# Fix keys with multiple names.
KEY_NAMES[evdev.ecodes.KEY_MUTE] = 'KEY_MUTE'
KEY_NAMES[evdev.ecodes.KEY_SCREENLOCK] = 'KEY_SCREENLOCK'
KEY_NAMES[evdev.ecodes.KEY_HANGEUL] = 'KEY_HANGEUL'
KEY_NAMES[evdev.ecodes.BTN_LEFT] = 'BTN_LEFT'
KEY_NAMES[evdev.ecodes.BTN_TRIGGER] = 'BTN_TRIGGER'
KEY_NAMES[evdev.ecodes.BTN_0] = 'BTN_0'
KEY_NAMES[evdev.ecodes.BTN_TOOL_PEN] = 'BTN_TOOL_PEN'


BITFIELDS = [
  ('B: EV=', evdev.ecodes.EV),
  ('B: PROP=', PROP_NAMES),
  ('B: KEY=', KEY_NAMES),
  ('B: REL=', evdev.ecodes.bytype[evdev.ecodes.EV_REL]),
  ('B: MSC=', evdev.ecodes.bytype[evdev.ecodes.EV_MSC]),
  ('B: LED=', evdev.ecodes.bytype[evdev.ecodes.EV_LED]),
  ('B: ABS=', evdev.ecodes.bytype[evdev.ecodes.EV_ABS]),
  ('B: SW=', evdev.ecodes.bytype[evdev.ecodes.EV_SW]),
  ('B: REP=', evdev.ecodes.bytype[evdev.ecodes.EV_REP]),
  ('B: SND=', evdev.ecodes.bytype[evdev.ecodes.EV_SND]),
]


def explain_bitfield(serialized_bits, bit_names, word_size, output_file):
  """Annotate a bitfield using the provided symbolic names for each bit."""

  bits = parse_bitfield(serialized_bits, word_size)
  for bit in bits:
    if bit in bit_names:
      output_file.write('    %s\n' % bit_names[bit])
    else:
      output_file.write('    0x%x\n' % bit)


def explain(input_file, output_file, word_size):
  """Annotate an input file formatted like /proc/bus/input/devices."""

  for line in input_file:
    output_file.write(line)
    for prefix, bit_names in BITFIELDS:
      if line.startswith(prefix):
        explain_bitfield(line[len(prefix):], bit_names, word_size, output_file)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('input_file', nargs='?',
      default='/proc/bus/input/devices',
      help="filename to read input from")
  parser.add_argument('output_file', nargs='?',
      help="filename to write output to")
  parser.add_argument('--word_size', type=int, default=64,
      help="word size of the system that generated the input file")
  args = parser.parse_args(argv)

  input_file = sys.stdin
  if args.input_file:
    input_file = open(args.input_file, 'r')

  output_file = sys.stdout
  if args.output_file:
    output_file = open(args.output_file, 'w')

  explain(input_file, output_file, args.word_size)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
