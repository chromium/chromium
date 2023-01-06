#!/usr/bin/env vpython3
#
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run benchmark for Instant start."""

from __future__ import print_function

import argparse
from datetime import datetime
import logging
import os
import pickle
import random
import re
import subprocess
import sys
import time

import stats.analyze


def get_timestamp(adb_log_line):
    """Parse the timestamp in the adb log"""
    # adb log doesn't have the year field printed out.
    parsed = datetime.strptime(adb_log_line[0:18], '%m-%d %H:%M:%S.%f')
    return parsed.replace(year=datetime.now().year)


def keep_awake():
    """Keep the device awake. This works for non-rooted devices as well."""
    os.system("adb shell svc power stayon true")
    os.system("adb shell input keyevent mouse")


def get_model():
    """Get the device model."""
    return subprocess.check_output(
        ['adb', 'shell', 'getprop', 'ro.product.model']).rstrip()


def run_apk(variant, dry_run=False, reinstall=False, check_state=False):
    """Run Chrome and return metrics"""

    keep_awake()

    variant_name, apk_script, extra_cmd = variant
    logging.warning('Running variant "%s"', variant_name)
    assert os.path.exists(apk_script), "Script '%s' doesn't exist" % apk_script

    features = '--enable-features=' + ','.join([
        'TabGroupsAndroid<Study', 'TabSwitcherOnReturn<Study',
        'StartSurfaceAndroid<Study', 'InstantStart<Study'
    ])

    args = '--args=' + ' '.join([
        '--disable-fre', '--disable-field-trial-config', features,
        '--force-fieldtrials=Study/Group',
        '--force-fieldtrial-params=Study.Group:'
        'start_surface_return_time_seconds/0'
        '/start_surface_variation/single'
        '/show_last_active_tab_only/true'
        '/open_ntp_instead_of_start/true'
    ] + extra_cmd)

    if reinstall:
        logging.warning('Uninstalling')
        cmd = [apk_script, 'uninstall']
        logging.info('Running %s', cmd)
        logging.info(subprocess.check_output(cmd, stderr=subprocess.STDOUT))

    # Use "unbuffer" to force flushing the output of |apk_script|.
    cmd = ['unbuffer', apk_script, 'run', '-vvv', args]
    logging.info('Running %s', cmd)
    # Use unbuffered pipe to avoid blocking.
    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            bufsize=0)
    latencies = []
    events_re = re.compile(
        r"Startup.Android.(?P<name>[0-9a-zA-Z_]+)[^ ]* = (?P<value>[0-9.]+)")

    # Avoid buffering in proc.stdout.next().
    # "for line in prod.stdout" might block.
    # See https://stackoverflow.com/questions/1183643/
    for line in iter(proc.stdout.readline, b''):
        if isinstance(line, bytes):
            line = line.decode(encoding='utf8')
        logging.debug(line.rstrip())
        if ('ActivityTaskManager' in line
                or 'ActivityManager' in line) and 'START' in line:
            start_timestamp = get_timestamp(line)
            logging.info('Chrome started at %s', start_timestamp)
            if dry_run:
                time.sleep(5)
                if check_state:
                    logging.warning('Make sure there is at least one tab, '
                                    'and the Feed is loaded. '
                                    'Press Enter to continue.')
                    sys.stdin.readline()
                break
        groups = events_re.search(line)
        if groups:
            latency = {}
            latency['variant_name'] = variant_name
            latency['metric_name'] = groups.group('name')
            latency['value'] = groups.group('value')
            latencies.append(latency)
            logging.info(line.rstrip())
            logging.info('Got %s = %s', groups.group('name'),
                         groups.group('value'))
        if len(latencies) >= 5:
            break

    proc.kill()
    return latencies


def main():
    """Entry point of the benchmark script"""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--control-apk',
                        default='out/Release/bin/monochrome_apk',
                        help='The APK script file for control behavior.')
    parser.add_argument('--experiment-apk',
                        default='out/Release/bin/monochrome_apk',
                        help='The APK script file for experiment behavior.')
    parser.add_argument('--reinstall',
                        action='store_true',
                        help='Uninstall before installing the APKs.')
    parser.add_argument('--repeat',
                        type=int,
                        default=3,
                        help='How many times to repeat running.')
    parser.add_argument('--data-output',
                        default='runs.pickle',
                        help='The output file for benchmark data.')
    parser.add_argument('-v',
                        '--verbose',
                        action='count',
                        default=0,
                        help='Be more verbose.')
    args, _ = parser.parse_known_args()

    level = logging.WARNING
    if args.verbose == 1:
        level = logging.INFO
    elif args.verbose >= 2:
        level = logging.DEBUG
    logging.basicConfig(level=level,
                        format='%(asctime)-2s %(levelname)-8s %(message)s')
    logging.addLevelName(
        logging.WARNING,
        "\033[1;31m%s\033[1;0m" % logging.getLevelName(logging.WARNING))
    logging.addLevelName(
        logging.ERROR,
        "\033[1;41m%s\033[1;0m" % logging.getLevelName(logging.ERROR))

    try:
        subprocess.check_output('which unbuffer', shell=True)
    except subprocess.CalledProcessError:
        sys.exit('ERROR: "unbuffer" not found. ' +
                 'Install by running "sudo apt install expect".')

    logging.warning('Make sure the device screen is unlocked. '
                    'Otherwise the benchmark might get stuck.')

    # List control/experiment APKs for side-by-side comparison.
    variants = []
    variants.append(('control', args.control_apk, []))
    variants.append(('experiment', args.experiment_apk, []))

    metadata = {'model': get_model(), 'start_time': datetime.now()}

    logging.warning('Pre-run for flag caching.')
    for variant in variants:
        run_apk(variant, dry_run=True, reinstall=args.reinstall)

    logging.warning('Dry-run for manual state checking.')
    for variant in variants:
        run_apk(variant, dry_run=True, check_state=True)

    runs = []
    for i in range(args.repeat):
        logging.warning('Run %d/%d', i + 1, args.repeat)
        random.shuffle(variants)
        for variant in variants:
            result = run_apk(variant)
            logging.info('Results: %s', result)
            runs.extend(result)
            time.sleep(10)  # try to avoid overloading the device.
        with open(args.data_output, 'wb') as pickle_file:
            pickle.dump(metadata, pickle_file)
            pickle.dump(runs, pickle_file)
            logging.info('Saved "%s"', args.data_output)
        stats.analyze.print_report(runs, metadata['model'])


if __name__ == '__main__':
    sys.exit(main())
