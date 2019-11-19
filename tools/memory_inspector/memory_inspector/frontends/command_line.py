# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command line frontend for Memory Inspector"""

from __future__ import print_function

import json
import memory_inspector
import optparse
import os
import time

from memory_inspector import constants
from memory_inspector.classification import mmap_classifier
from memory_inspector.core import backends
from memory_inspector.data import serialization


def main():
  COMMANDS = ['devices', 'ps', 'stats', 'mmaps', 'classified_mmaps']
  usage = ('%prog [options] ' + ' | '.join(COMMANDS))
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-b', '--backend', help='Backend name '
                    '(e.g., Android)', type='string', default='Android')
  parser.add_option('-s', '--device_id', help='Device '
                    'id (e.g., Android serial)', type='string')
  parser.add_option('-p', '--process_id', help='Target process id',
                    type='int')
  parser.add_option('-m', '--filter_process_name', help='Process '
                    'name to match', type='string')
  parser.add_option('-r', '--mmap_rule',
                    help='mmap rule', type='string',
                    default=os.path.join(constants.CLASSIFICATION_RULES_PATH,
                        'default', 'mmap-android.py'))
  (options, args) = parser.parse_args()

  memory_inspector.RegisterAllBackends()

  if not args or args[0] not in COMMANDS:
    parser.print_help()
    return -1

  if args[0] == 'devices':
    _ListDevices(options.backend)
    return 0

  number_of_devices = 0
  if options.device_id:
    device_id = options.device_id
    number_of_devices = 1
  else:
    for device in backends.ListDevices():
      if device.backend.name == options.backend:
        number_of_devices += 1
        device_id = device.id

  if number_of_devices == 0:
    print("No devices connected")
    return -1

  if number_of_devices > 1:
    print ('More than 1 device connected. You need to provide'
        ' --device_id')
    return -1

  device = backends.GetDevice(options.backend, device_id)
  if not device:
    print('Device', device_id, 'does not exist')
    return -1

  device.Initialize()
  if args[0] == 'ps':
    if not options.filter_process_name:
      print('Listing all processes')
    else:
      print('Listing processes matching ' + options.filter_process_name.lower())
    print('')
    print('%-10s : %-50s : %12s %12s %12s' %
          ('Process ID', 'Process Name', 'RUN_TIME', 'THREADS', 'MEM_RSS_KB'))
    print('')
    for process in device.ListProcesses():
      if (not options.filter_process_name or
          options.filter_process_name.lower() in process.name.lower()):
        stats = process.GetStats()
        run_time_min, run_time_sec = divmod(stats.run_time, 60)
        print('%10s : %-50s : %6s m %2s s %8s %12s' %
              (process.pid, _Truncate(process.name, 50), run_time_min,
               run_time_sec, stats.threads, stats.vm_rss))
    return 0

  if not options.process_id:
    print('You need to provide --process_id')
    return -1

  process = device.GetProcess(options.process_id)

  if not process:
    print('Cannot find process [%d] on device %s' % (options.process_id,
                                                     device.id))
    return -1
  elif args[0] == 'stats':
    _ListProcessStats(process)
    return 0
  elif args[0] == 'mmaps':
    _ListProcessMmaps(process)
    return 0
  elif args[0] == 'classified_mmaps':
    _ListProcessClassifiedMmaps(process, options.mmap_rule)
    return 0


def _ListDevices(backend_name):
  print('Device list:')
  print('')
  for device in backends.ListDevices():
    if device.backend.name == backend_name:
      print('%-16s : %s' % (device.id, device.name))


def _ListProcessStats(process):
  """Prints process stats periodically
  """
  print('Stats for process: [%d] %s' % (process.pid, process.name))
  print('%-10s : %-50s : %12s %12s %13s %12s %14s' %
        ('Process ID', 'Process Name', 'RUN_TIME', 'THREADS', 'CPU_USAGE',
         'MEM_RSS_KB', 'PAGE_FAULTS'))
  print('')
  while True:
    stats = process.GetStats()
    run_time_min, run_time_sec = divmod(stats.run_time, 60)
    print('%10s : %-50s : %6s m %2s s %8s %12s %13s %11s' %
          (process.pid, _Truncate(process.name, 50), run_time_min, run_time_sec,
           stats.threads, stats.cpu_usage, stats.vm_rss, stats.page_faults))
    time.sleep(1)


def _ListProcessMmaps(process):
  """Prints process memory maps
  """
  print('Memory Maps for process: [%d] %s' % (process.pid, process.name))
  print('%-10s %-10s %6s %12s %12s %13s %13s %-40s' %
        ('START', 'END', 'FLAGS', 'PRIV.DIRTY', 'PRIV.CLEAN', 'SHARED DIRTY',
         'SHARED CLEAN', 'MAPPED_FILE'))
  print('%38s %12s %12s %13s' % ('(kb)', '(kb)', '(kb)', '(kb)'))
  print('')
  maps = process.DumpMemoryMaps()
  for entry in maps.entries:
    print('%-10x %-10x %6s %12s %12s %13s %13s %-40s' % (
        entry.start, entry.end, entry.prot_flags, entry.priv_dirty_bytes / 1024,
        entry.priv_clean_bytes / 1024, entry.shared_dirty_bytes / 1024,
        entry.shared_clean_bytes / 1024, entry.mapped_file))


def _ListProcessClassifiedMmaps(process, mmap_rule):
  """Prints process classified memory maps
  """
  maps = process.DumpMemoryMaps()
  if not os.path.exists(mmap_rule):
    print('File', mmap_rule, 'not found')
    return
  with open(mmap_rule) as f:
    rules = mmap_classifier.LoadRules(f.read())
  classified_results_tree =  mmap_classifier.Classify(maps, rules)
  print(json.dumps(classified_results_tree, cls=serialization.Encoder))


def _Truncate(name, max_length):
  if len(name) <= max_length:
    return name
  return '%s...' % name[0:(max_length - 3)]
