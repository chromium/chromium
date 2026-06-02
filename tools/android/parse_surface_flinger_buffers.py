#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import re
import sys

_SRC_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils
from devil.android.sdk import adb_wrapper

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium


def get_pid_map(device):
  pid_map = {}
  if not device:
    return pid_map

  try:
    processes = device.ListProcesses()
    for p in processes:
      pid_map[str(p.pid)] = p.name
  except Exception as e:
    logging.warning("Failed to get process list via Devil: %s", e)
  return pid_map


def build_sf_maps(sf_output):
  window_to_package = {}
  task_to_package = {}

  pkg_act_pattern = re.compile(
      r'(?:VRI-)?(?P<pkg>[a-zA-Z0-9_\.]+)/(?P<act>[^#\s]+)#\d+')
  assert pkg_act_pattern.search('Layer [3458] VRI-org.chromium.chrome/'
                                'com.google.android.apps.chrome.Main#3458')
  assert pkg_act_pattern.search('Layer [24361] com.chrome.dev/'
                                'com.google.android.apps.chrome.Main#24361')

  act_pattern = re.compile(r'ActivityRecord\{[a-f0-9]+\s+u\d+\s+'
                           r'(?P<pkg_act>[^\s\}]+)\s+t(?P<task_id>\d+)\}')
  assert act_pattern.search(
      'ActivityRecord{35469639 u10 org.chro[...]roid.apps.chrome.Main '
      't1000049}')

  for line in sf_output.splitlines():
    pkg_act_match = pkg_act_pattern.search(line)
    if pkg_act_match:
      pkg = pkg_act_match.group('pkg')
      act = pkg_act_match.group('act')
      short_name = act.split('.')[-1]
      window_to_package[short_name] = pkg
      continue

    act_match = act_pattern.search(line)
    if act_match:
      pkg_act = act_match.group('pkg_act')
      task_id = act_match.group('task_id')
      short_name = pkg_act.split('.')[-1]
      if short_name in window_to_package:
        task_to_package[task_id] = window_to_package[short_name]

  return window_to_package, task_to_package


def parse_size_to_kib(size_str):
  size_str = size_str.strip()
  match = re.match(r'^(?P<val>[\d\.]+)\s*(?P<unit>[a-zA-Z]*)$', size_str)
  if not match:
    return 0.0

  val = float(match.group('val'))
  unit = match.group('unit').lower()

  if unit in ('kib', 'kb'):
    return val
  if unit in ('mib', 'mb'):
    return val * 1024.0
  if unit in ('gib', 'gb'):
    return val * 1024.0 * 1024.0
  if unit == 'b':
    return val / 1024.0
  return val


def parse_sf_buffers(pid_map, sf_output, filter_str=None):
  window_to_package, task_to_package = build_sf_maps(sf_output)

  in_gralloc_section = False
  buffers = []

  buffer_pattern = re.compile(
      r'^\+\s*name:(?P<name>[^,]+),\s*id:(?P<id>\d+),\s*size:(?P<size>[^,]+),'
      r'\s*w/h:(?P<wh>[^,]+),\s*usage:\s*(?P<usage>[^,]+)')

  total_line = ""
  for line in sf_output.splitlines():
    if "Imported gralloc buffers:" in line:
      in_gralloc_section = True
      continue
    if in_gralloc_section and "Total imported by gralloc:" in line:
      total_line = line.strip()
      in_gralloc_section = False
      break

    if in_gralloc_section:
      match = buffer_pattern.match(line.strip())
      if match:
        buf_info = match.groupdict()
        name = buf_info['name']

        # Try to resolve PID
        pid_match = re.search(r'pid\s*\[(\d+)\]', name)
        resolved_owner = "Unknown"
        if pid_match:
          pid = pid_match.group(1)
          resolved_owner = pid_map.get(pid, f"PID {pid}")
        elif "VRI[" in name:
          vri_match = re.search(r'VRI\[([^\]]+)\]', name)
          if vri_match:
            vri_name = vri_match.group(1)
            task_match = re.search(r'Task=(\d+)', vri_name)
            if task_match and task_match.group(1) in task_to_package:
              resolved_owner = (f"{task_to_package[task_match.group(1)]} "
                                f"(Window: {vri_name})")
            elif vri_name in window_to_package:
              resolved_owner = (
                  f"{window_to_package[vri_name]} (Window: {vri_name})")
            else:
              resolved_owner = f"Window: {vri_name}"
        else:
          resolved_owner = name.split('#')[0]

        buf_info['resolved_owner'] = resolved_owner

        if filter_str:
          filter_lower = filter_str.lower()
          if (filter_lower in resolved_owner.lower()
              or filter_lower in name.lower()):
            buffers.append(buf_info)
        else:
          buffers.append(buf_info)

  if filter_str:
    print(f"Filtered by: '{filter_str}'")
  print(f"\n{total_line}\n")
  print(f"{'Owner / Process / Window':<60} | "
        f"{'Buffer Name':<45} | {'ID':<6} | {'Size':<12} | {'W x H':<12}")
  print("-" * 145)

  buffers.sort(key=lambda x: x['resolved_owner'])

  for buf in buffers:
    display_name = buf['name']
    if len(display_name) > 42:
      display_name = display_name[:40] + ".."

    print(f"{buf['resolved_owner']:<60} | {display_name:<45} | "
          f"{buf['id']:<6} | {buf['size']:<12} | {buf['wh']:<12}")

  if filter_str:
    total_size = sum(parse_size_to_kib(buf['size']) for buf in buffers)
    print("-" * 145)
    print(f"Total size of filtered buffers: {total_size:.2f} KiB "
          f"({total_size / 1024.0:.2f} MiB)")


def main():
  parser = argparse.ArgumentParser(
      description="Parse graphics buffers from SurfaceFlinger dumpsys.")
  parser.add_argument(
      "--file",
      help="Path to a local file containing SurfaceFlinger dumpsys output "
      "(instead of querying device)")
  parser.add_argument(
      "--filter",
      help="Filter results by owner or buffer name (case-insensitive)")
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  sf_output = ""
  pid_map = {}

  if args.file:
    logging.info("Reading SurfaceFlinger dump from file: %s", args.file)
    try:
      with open(args.file, 'r', encoding='utf-8') as f:
        sf_output = f.read()
    except Exception as e:
      logging.error("Error reading file %s: %s", args.file, e)
      sys.exit(1)
  else:
    devil_chromium.Initialize()
    devices = device_utils.DeviceUtils.HealthyDevices()
    if not devices:
      logging.error("No healthy devices found")
      sys.exit(1)
    device = devices[0]

    logging.info("Gathering PID map from live device via Devil...")
    pid_map = get_pid_map(device)

    logging.info("Running dumpsys SurfaceFlinger via Devil...")
    try:
      sf_lines = device.RunShellCommand(['dumpsys', 'SurfaceFlinger'],
                                        check_return=True,
                                        large_output=True)
      sf_output = "\n".join(sf_lines)
    except Exception as e:
      logging.error("Error running dumpsys SurfaceFlinger: %s", e)
      sys.exit(1)

  parse_sf_buffers(pid_map, sf_output, args.filter)


if __name__ == "__main__":
  main()
