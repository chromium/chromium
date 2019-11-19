#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Symbolizes and prints live objects as recorded by tcmalloc's
HeapProfilerDumpLiveObjects.
"""

from __future__ import print_function

import os
import re
import subprocess
import sys
import tempfile

def usage():
  print("""\
Usage:
  tools/tcmalloc/print-live-objects.py out/Debug/chrome leaks.dmp
""")


def LoadDump(dump_file):
  result = []
  leakfmt = re.compile(
      r"^\s*1:\s*(\d+)\s*\[\s*1:\s*\d+\]\s*@(0x[a-f0-9]+)((\s+0x[a-f0-9]+)*)$")
  line_no = 0
  with open(dump_file) as f:
    for line in f:
      line_no = line_no + 1
      matches = leakfmt.match(line)
      if not matches:
        print("%s: could not parse line %d, skipping" % (dump_file, line_no))
      else:
        trace = { "size": int(matches.group(1)),
                  "address": matches.group(2),
                  "frames": matches.group(3).strip().split(" ")}
        result.append(trace)
  return result


def Symbolize(binary, traces):
  addresses = set()
  for trace in traces:
    for frame in trace["frames"]:
      addresses.add(frame)
  addr_file, addr_filename = tempfile.mkstemp()
  for addr in addresses:
    os.write(addr_file, "%s\n" % addr)
  os.close(addr_file)
  syms = subprocess.Popen([
      "addr2line", "-f", "-C", "-e", binary, "@%s" % addr_filename],
      stdout=subprocess.PIPE).communicate()[0].strip().split("\n")
  table = {}
  cwd = os.getcwd()
  for address, symbol, location in zip(addresses, syms[::2], syms[1::2]):
    if location != "??:0":
      filename, line = location.split(":")
      filename = os.path.realpath(filename)[len(cwd)+1:]
      location = "%s:%s" % (filename, line)
    table[address] = { "name": symbol, "location": location }
  for trace in traces:
    frames = []
    for frame in trace["frames"]:
      frames.append(table[frame])
    trace["frames"] = frames


def Main(argv):
  if sys.platform != 'linux2':
    print('print-live-objects.py requires addr2line only present on Linux.')
    sys.exit(1)

  if len(argv) != 3:
    usage()
    sys.exit(1)

  traces = LoadDump(argv[2])
  Symbolize(argv[1], traces)

  if not traces:
    print("No leaks found!")

  for trace in sorted(traces, key=lambda x: -x["size"]):
    print("Leak of %d bytes at address %s" % (trace["size"], trace["address"]))
    for frame in trace["frames"]:
      print("  %s (%s)" % (frame["name"], frame["location"]))
    print("")


if __name__ == '__main__':
  Main(sys.argv)
