#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Summarizes graphics memory usage on an Android device via adb shell.

Parses data from:
  - adb shell dumpsys gpu
  - adb shell dmabuf_dump
  - adb shell dumpsys SurfaceFlinger
"""

import argparse
import logging
import os
import re
import sys

_SRC_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'catapult', 'devil'))
from devil.android import device_errors
from devil.android import device_utils

sys.path.append(os.path.join(_SRC_PATH, 'build', 'android'))
import devil_chromium


def _run_adb_shell_command(device, cmd):
  if not device:
    return ""
  try:
    return device.RunShellCommand(
      cmd, check_return=True, large_output=True, raw_output=True
    )
  except device_errors.CommandFailedError as e:
    logging.error("Failed to run adb command %s on device: %s", cmd, e)
    return ""


def _parse_size_to_bytes(size_val, unit_str=''):
  unit = unit_str.strip().lower()
  val = float(size_val)
  if unit in ('kib', 'kb'):
    return val * 1024.0
  elif unit in ('mib', 'mb'):
    return val * 1024.0 * 1024.0
  elif unit in ('gib', 'gb'):
    return val * 1024.0 * 1024.0 * 1024.0
  elif unit in ('b', 'bytes', ''):
    return val
  return val


def _format_bytes(num_bytes):
  if num_bytes is None:
    return "N/A"
  num = float(num_bytes)
  if num >= 1024.0 * 1024.0 * 1024.0:
    return f"{num / (1024.0**3):.2f} GiB"
  elif num >= 1024.0 * 1024.0:
    return f"{num / (1024.0**2):.2f} MiB"
  elif num >= 1024.0:
    return f"{num / 1024.0:.2f} KiB"
  else:
    return f"{int(num)} B"


def _get_pid_to_name_map(device):
  if not device:
    return {}
  pid_map = {}
  try:
    for proc in device.ListProcesses():
      pid_map[str(proc.pid)] = proc.name
  except Exception as e:
    logging.warning("Failed to get process list via Devil: %s", e)
  return pid_map


def _build_sf_maps(sf_output):
  """Builds window-to-package and task-to-package mappings from
  SurfaceFlinger.
  """
  window_to_package = {}
  task_to_package = {}

  pkg_act_pattern = re.compile(
    r'(?:VRI-)?(?P<pkg>[a-zA-Z0-9_\.]+)/(?P<act>[^#\s]+)#\d+'
  )
  act_pattern = re.compile(
    r'ActivityRecord\{[a-f0-9]+\s+u\d+\s+(?P<pkg_act>[^\s\}]+)\s+t(?P<task_id>\d+)\}'
  )

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


def _parse_dumpsys_gpu(content):
  """Parses 'dumpsys gpu' output."""
  result = {
    'global_total_bytes': None,
    'global_raw_line': None,
    'loaded_packages': [],
  }

  for line in content.splitlines():
    if 'global' in line.lower():
      result['global_raw_line'] = line.strip()
      m = re.search(r'(\d+)', line)
      if m:
        result['global_total_bytes'] = int(m.group(1))

    m_pkg = re.search(r'appPackageName\s*=\s*(.+)', line)
    if m_pkg:
      pkg = m_pkg.group(1).strip()
      if pkg and pkg not in result['loaded_packages']:
        result['loaded_packages'].append(pkg)

  return result


def _parse_dmabuf_dump(content, pid_map=None):
  """Parses 'dmabuf_dump' output."""
  if pid_map is None:
    pid_map = {}

  dmabuf_total_pattern = re.compile(
    r'dmabuf total:\s*(\d+)\s*kB\s+kernel_rss:\s*(\d+)\s*kB\s+userspace_rss:\s*(\d+)\s*kB\s+userspace_pss:\s*(\d+)\s*kB'
  )
  process_header_pattern = re.compile(
    r'^([a-zA-Z0-9_\.\-\/\[\]\(\)\ \:\#]+)\:(\d+)$'
  )
  process_total_pattern = re.compile(r'PROCESS TOTAL\s+(\d+)\s*kB\s+(\d+)\s*kB')

  result = {
    'total_kb': None,
    'kernel_rss_kb': None,
    'userspace_rss_kb': None,
    'userspace_pss_kb': None,
    'processes': [],
  }

  # Overall total line
  tot_match = dmabuf_total_pattern.search(content)
  if tot_match:
    result['total_kb'] = int(tot_match.group(1))
    result['kernel_rss_kb'] = int(tot_match.group(2))
    result['userspace_rss_kb'] = int(tot_match.group(3))
    result['userspace_pss_kb'] = int(tot_match.group(4))

  # Per-process breakdown
  process_blocks = re.split(r'-{5,}', content)
  for block in process_blocks:
    lines = [l.strip() for l in block.strip().splitlines() if l.strip()]
    if not lines:
      continue

    header_m = process_header_pattern.search(lines[0])
    proc_total_m = process_total_pattern.search(block)

    if header_m and proc_total_m:
      proc_name = header_m.group(1).strip()
      pid_str = header_m.group(2)
      rss_kb = int(proc_total_m.group(1))
      pss_kb = int(proc_total_m.group(2))

      resolved_name = pid_map.get(pid_str, proc_name)

      result['processes'].append(
        {
          'pid': int(pid_str),
          'name': proc_name,
          'resolved_name': resolved_name,
          'rss_bytes': rss_kb * 1024,
          'pss_bytes': pss_kb * 1024,
        }
      )

  result['processes'].sort(key=lambda x: x['pss_bytes'], reverse=True)
  return result


def _parse_dumpsys_surfaceflinger(content, pid_map=None):
  """Parses 'dumpsys SurfaceFlinger' output."""
  if pid_map is None:
    pid_map = {}

  window_to_package, task_to_package = _build_sf_maps(content)

  gralloc_total_pattern = re.compile(
    r'Total imported by gralloc:\s*([\d\.]+)\s*(\w+)'
  )
  sf_buffer_pattern = re.compile(
    r'^\+\s*name:\s*(?P<name>[^,]+),\s*id:\s*(?P<id>\d+),\s*size:\s*(?P<size>[^,]+)'
    r'(?:,\s*w/h:\s*(?P<wh>[^,]+))?'
  )
  size_unit_pattern = re.compile(r'^(?P<val>[\d\.]+)\s*(?P<unit>[a-zA-Z]*)$')

  result = {
    'gralloc_total_bytes': None,
    'imported_buffers_by_name': {},
    'allocator_buffers_by_requestor': {},
    'itemized_buffers': [],
  }

  # Total imported by gralloc line
  gralloc_m = gralloc_total_pattern.search(content)
  if gralloc_m:
    size_val = float(gralloc_m.group(1))
    unit_str = gralloc_m.group(2)
    result['gralloc_total_bytes'] = _parse_size_to_bytes(size_val, unit_str)

  # Imported gralloc buffers section
  in_imported = False
  imported_agg = {}

  for line in content.splitlines():
    if 'Imported gralloc buffers:' in line:
      in_imported = True
      continue
    if in_imported and (
      'Total imported by gralloc:' in line
      or 'GraphicBufferAllocator buffers:' in line
      or 'FlagManager' in line
    ):
      in_imported = False
      continue

    if in_imported:
      m = sf_buffer_pattern.match(line.strip())
      if m:
        name = m.group('name').strip()
        buf_id = m.group('id').strip()
        size_str = m.group('size').strip()
        wh_str = m.group('wh').strip() if m.group('wh') else "N/A"
        sz_m = size_unit_pattern.match(size_str)

        size_bytes = 0.0
        if sz_m:
          size_bytes = _parse_size_to_bytes(
            sz_m.group('val'), sz_m.group('unit')
          )

        # Attempt resolving owner name
        resolved_owner = name
        pid_m = re.search(r'pid\s*\[(\d+)\]', name)
        if pid_m and pid_m.group(1) in pid_map:
          resolved_owner = f"{pid_map[pid_m.group(1)]} ({name})"
        elif "VRI[" in name:
          vri_match = re.search(r'VRI\[([^\]]+)\]', name)
          if vri_match:
            vri_name = vri_match.group(1)
            task_match = re.search(r'Task=(\d+)', vri_name)
            if task_match and task_match.group(1) in task_to_package:
              resolved_owner = (
                f"{task_to_package[task_match.group(1)]} (Window: {vri_name})"
              )
            elif vri_name in window_to_package:
              resolved_owner = (
                f"{window_to_package[vri_name]} (Window: {vri_name})"
              )
            else:
              resolved_owner = f"Window: {vri_name}"
        else:
          resolved_owner = name.split('#')[0]

        if resolved_owner not in imported_agg:
          imported_agg[resolved_owner] = {
            'count': 0,
            'total_bytes': 0.0,
            'raw_name': name,
          }
        imported_agg[resolved_owner]['count'] += 1
        imported_agg[resolved_owner]['total_bytes'] += size_bytes

        result['itemized_buffers'].append(
          {
            'name': name,
            'id': buf_id,
            'size_bytes': size_bytes,
            'wh': wh_str,
            'resolved_owner': resolved_owner,
          }
        )

  result['imported_buffers_by_name'] = imported_agg

  # GraphicBufferAllocator buffers section
  in_gba = False
  gba_agg = {}

  for line in content.splitlines():
    if 'GraphicBufferAllocator buffers:' in line:
      in_gba = True
      continue
    if in_gba:
      if (
        'Total imported by gralloc:' in line
        or 'FlagManager' in line
        or not line.strip()
      ):
        if 'Total imported by gralloc:' in line or 'FlagManager' in line:
          in_gba = False
        continue

      parts = [p.strip() for p in line.split('|')]
      if len(parts) >= 7 and not parts[0].startswith('Handle'):
        size_str = parts[1]
        requestor = parts[6]

        sz_m = size_unit_pattern.match(size_str)
        size_bytes = 0.0
        if sz_m:
          size_bytes = _parse_size_to_bytes(
            sz_m.group('val'), sz_m.group('unit')
          )

        if requestor not in gba_agg:
          gba_agg[requestor] = {'count': 0, 'total_bytes': 0.0}
        gba_agg[requestor]['count'] += 1
        gba_agg[requestor]['total_bytes'] += size_bytes

  result['allocator_buffers_by_requestor'] = gba_agg
  return result


def _print_summary(gpu_data, dmabuf_data, sf_data, filter_str=None):
  """Prints formatted summary tables to stdout."""
  print("\n" + "=" * 80)
  print("         ANDROID GRAPHICS MEMORY USAGE SUMMARY")
  print("=" * 80)

  # Executive Totals Table
  print("\n[ 1. OVERALL GRAPHICS MEMORY TOTALS ]")
  print("-" * 80)
  print(f"{'Source / Component':<35} | {'Metric / Memory':<40}")
  print("-" * 80)

  gpu_global = gpu_data.get('global_total_bytes')
  gpu_str = _format_bytes(gpu_global)
  print(f"{'dumpsys gpu (Global total)':<35} | {gpu_str:<40}")

  dmabuf_pss = (
    dmabuf_data.get('userspace_pss_kb') * 1024
    if dmabuf_data.get('userspace_pss_kb') is not None
    else None
  )
  dmabuf_total = (
    dmabuf_data.get('total_kb') * 1024
    if dmabuf_data.get('total_kb') is not None
    else None
  )
  dmabuf_rss = (
    dmabuf_data.get('userspace_rss_kb') * 1024
    if dmabuf_data.get('userspace_rss_kb') is not None
    else None
  )

  print(
    f"{'dmabuf_dump (Userspace PSS)':<35} | {_format_bytes(dmabuf_pss):<40}"
  )
  print(
    f"{'dmabuf_dump (Userspace RSS)':<35} | {_format_bytes(dmabuf_rss):<40}"
  )
  print(
    f"{'dmabuf_dump (Kernel DMA-BUF Total)':<35} |"
    f" {_format_bytes(dmabuf_total):<40}"
  )

  sf_gralloc = sf_data.get('gralloc_total_bytes')
  print(
    f"{'dumpsys SurfaceFlinger (Gralloc)':<35} |"
    f" {_format_bytes(sf_gralloc):<40}"
  )
  print("-" * 80)

  # dmabuf_dump breakdown
  print("\n[ 2. DMABUF_DUMP PER-PROCESS BREAKDOWN (by PSS) ]")
  print("-" * 80)
  print(
    f"{'PID':<7} | {'Process Name / Package':<38} | {'PSS':<14} | {'RSS':<14}"
  )
  print("-" * 80)

  procs = dmabuf_data.get('processes', [])
  if filter_str:
    procs = [
      p
      for p in procs
      if filter_str.lower() in p['resolved_name'].lower()
      or filter_str.lower() in p['name'].lower()
    ]

  for p in procs:
    p_name = p['resolved_name']
    if len(p_name) > 36:
      p_name = p_name[:34] + ".."
    print(
      f"{p['pid']:<7} | {p_name:<38} | {_format_bytes(p['pss_bytes']):<14} |"
      f" {_format_bytes(p['rss_bytes']):<14}"
    )
  print("-" * 80)

  # SurfaceFlinger breakdown
  print("\n[ 3. SURFACEFLINGER IMPORTED GRALLOC BUFFERS SUMMARY ]")
  print("-" * 80)
  print(f"{'Buffer Name / Owner':<45} | {'Count':<7} | {'Total Memory':<18}")
  print("-" * 80)

  imported = sf_data.get('imported_buffers_by_name', {})
  sorted_imported = sorted(
    imported.items(), key=lambda x: x[1]['total_bytes'], reverse=True
  )

  if filter_str:
    sorted_imported = [
      item for item in sorted_imported if filter_str.lower() in item[0].lower()
    ]

  for name, data in sorted_imported:
    b_name = name
    if len(b_name) > 43:
      b_name = b_name[:41] + ".."
    print(
      f"{b_name:<45} | {data['count']:<7} |"
      f" {_format_bytes(data['total_bytes']):<18}"
    )
  print("-" * 80)

  # Itemized SurfaceFlinger buffers table
  itemized = sf_data.get('itemized_buffers', [])
  if itemized:
    print("\n[ 4. SURFACEFLINGER ITEMIZED BUFFERS LIST ]")
    print("-" * 145)
    print(
      f"{'Owner / Process / Window':<60} | "
      f"{'Buffer Name':<45} | {'ID':<6} | {'Size':<12} | {'W x H':<12}"
    )
    print("-" * 145)

    if filter_str:
      itemized = [
        b
        for b in itemized
        if filter_str.lower() in b['resolved_owner'].lower()
        or filter_str.lower() in b['name'].lower()
      ]

    itemized.sort(key=lambda x: x['resolved_owner'])

    for buf in itemized:
      owner_str = buf['resolved_owner']
      if len(owner_str) > 58:
        owner_str = owner_str[:56] + ".."
      display_name = buf['name']
      if len(display_name) > 43:
        display_name = display_name[:41] + ".."

      print(
        f"{owner_str:<60} | {display_name:<45} | "
        f"{buf['id']:<6} | {_format_bytes(buf['size_bytes']):<12} | "
        f"{buf['wh']:<12}"
      )
    print("-" * 145)

  # dumpsys gpu details
  print("\n[ 5. DUMPSYS GPU DETAILS ]")
  print("-" * 80)
  if gpu_data.get('global_raw_line'):
    print(f"Global line: {gpu_data['global_raw_line']}")
  pkgs = gpu_data.get('loaded_packages', [])
  if pkgs:
    print(f"Active GPU App Packages ({len(pkgs)}): {', '.join(pkgs)}")
  print("-" * 80 + "\n")


def main():
  parser = argparse.ArgumentParser(
    description=(
      "Parses dumpsys gpu, dmabuf_dump, and dumpsys SurfaceFlinger to"
      " summarize graphics memory usage on an Android device."
    )
  )
  parser.add_argument(
    "-s",
    "--serial",
    "-d",
    "--device",
    dest="device",
    help="Target ADB device serial (for multiple connected devices)",
  )
  parser.add_argument(
    "--filter",
    help=(
      "Filter results by process or buffer name substring (case-insensitive)"
    ),
  )

  args = parser.parse_args()
  logging.basicConfig(
    level=logging.INFO,
    format="%(levelname)s: %(message)s",
    stream=sys.stderr,
  )

  devil_chromium.Initialize()
  devices = device_utils.DeviceUtils.HealthyDevices(device_arg=args.device)
  if not devices:
    logging.error("No healthy devices found")
    return 1
  if len(devices) > 1 and not args.device:
    logging.warning("Multiple devices attached. Using %s.", str(devices[0]))
  device = devices[0]
  try:
    device.EnableRoot()
  except (
    device_errors.CommandFailedError,
    device_errors.RootUserBuildError,
  ) as e:
    logging.warning("Unable to enable root on device: %s", e)

  logging.info("Querying device via 'dumpsys gpu'...")
  gpu_content = _run_adb_shell_command(device, ['dumpsys', 'gpu'])

  logging.info("Querying device via 'dmabuf_dump'...")
  dmabuf_content = _run_adb_shell_command(device, ['dmabuf_dump'])

  logging.info("Querying device via 'dumpsys SurfaceFlinger'...")
  sf_content = _run_adb_shell_command(device, ['dumpsys', 'SurfaceFlinger'])

  logging.info("Fetching process PID mapping from device...")
  pid_map = _get_pid_to_name_map(device)

  # Parse data
  gpu_data = _parse_dumpsys_gpu(gpu_content)
  dmabuf_data = _parse_dmabuf_dump(dmabuf_content, pid_map)
  sf_data = _parse_dumpsys_surfaceflinger(sf_content, pid_map)

  _print_summary(
    gpu_data,
    dmabuf_data,
    sf_data,
    filter_str=args.filter,
  )
  return 0


if __name__ == "__main__":
  sys.exit(main())
