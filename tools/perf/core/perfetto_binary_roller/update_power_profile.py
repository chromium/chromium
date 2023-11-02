#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import datetime
import os
import sys
import xml.etree.ElementTree as ET

# Add tools/perf to sys.path.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))

from core import path_util
path_util.AddPyUtilsToPath()
path_util.AddTracingToPath()

from core.perfetto_binary_roller import binary_deps_manager
from core.tbmv3 import trace_processor
from py_utils import tempfile_ext


def ExtractValues(xml_path):
  root = ET.parse(xml_path).getroot()

  speeds = []
  power = []
  clusters = []
  for array in root.iter('array'):
    if array.get('name') == 'cpu.clusters.cores':
      clusters = [int(value.text) for value in array.iter('value')]
    if array.get('name').startswith('cpu.core_speeds.'):
      speeds.append([int(value.text) for value in array.iter('value')])
    if array.get('name').startswith('cpu.core_power.'):
      power.append([float(value.text) for value in array.iter('value')])

  values = []
  cpu = 0
  for cluster, n_cpus in enumerate(clusters):
    for _ in range(n_cpus):
      for freq, drain in zip(speeds[cluster], power[cluster]):
        # TODO(khokhlov): Remove the correction when power profiles are updated.
        corrected_drain = drain / n_cpus
        values.append((cpu, cluster, freq, corrected_drain))
      cpu += 1

  return values


def ExportProfiles(device_xmls, sql_path):
  sql_values = []
  for device, xml_path in device_xmls:
    sql_values += [
        '("%s", %s, %s, %s, %s)' % ((device, ) + v)
        for v in ExtractValues(xml_path)
    ]

  with open(sql_path, 'w') as sql_file:
    sql_file.write('INSERT INTO power_profile VALUES\n')
    sql_file.write(',\n'.join(sql_values))
    sql_file.write(';\n')


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--device-xml',
      nargs=2,
      metavar=('DEVICE', 'XML_FILE'),
      action='append',
      help='Device name and path to the XML file with the device '
      'power profile. Can be used multiple times.')

  args = parser.parse_args(args)

  with tempfile_ext.NamedTemporaryDirectory() as tempdir:
    sql_path = os.path.join(tempdir, trace_processor.POWER_PROFILE_SQL)
    ExportProfiles(args.device_xml, sql_path)
    version = datetime.datetime.now().strftime('%Y%m%dT%H%M%S')
    binary_deps_manager.UploadAndSwitchDataFile(
        trace_processor.POWER_PROFILE_SQL, sql_path, version)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
