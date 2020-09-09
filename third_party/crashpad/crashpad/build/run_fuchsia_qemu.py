#!/usr/bin/env python

# Copyright 2017 The Crashpad Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Helper script to [re]start or stop a helper Fuchsia QEMU instance to be used
for running tests without a device.
"""

from __future__ import print_function

import getpass
import os
import random
import signal
import subprocess
import sys
import tempfile
import time

try:
    from subprocess import DEVNULL
except ImportError:
    DEVNULL = open(os.devnull, 'r+b')

CRASHPAD_ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             os.pardir)


def _Stop(pid_file):
    if os.path.isfile(pid_file):
        with open(pid_file, 'rb') as f:
            pid = int(f.read().strip())
        try:
            os.kill(pid, signal.SIGTERM)
        except:
            print('Unable to kill pid %d, continuing' % pid, file=sys.stderr)
        os.unlink(pid_file)


def _CheckForTun():
    """Check for networking. TODO(scottmg): Currently, this is Linux-specific.
    """
    returncode = subprocess.call(
        ['tunctl', '-b', '-u',
         getpass.getuser(), '-t', 'qemu'],
        stdout=DEVNULL,
        stderr=DEVNULL)
    if returncode != 0:
        print('To use QEMU with networking on Linux, configure TUN/TAP. See:',
              file=sys.stderr)
        print(
            '  https://fuchsia.googlesource.com/zircon/+/HEAD/docs/qemu.md#enabling-networking-under-qemu-x86_64-only',
            file=sys.stderr)
        return 2
    return 0


def _Start(pid_file):
    tun_result = _CheckForTun()
    if tun_result != 0:
        return tun_result

    arch = 'mac-amd64' if sys.platform == 'darwin' else 'linux-amd64'
    fuchsia_dir = os.path.join(CRASHPAD_ROOT, 'third_party', 'fuchsia')
    qemu_path = os.path.join(fuchsia_dir, 'qemu', arch, 'bin',
                             'qemu-system-x86_64')
    kernel_data_dir = os.path.join(fuchsia_dir, 'sdk', arch, 'target', 'x86_64')
    kernel_path = os.path.join(kernel_data_dir, 'zircon.bin')
    initrd_path = os.path.join(kernel_data_dir, 'bootdata.bin')

    mac_tail = ':'.join('%02x' % random.randint(0, 255) for x in range(3))
    instance_name = (
        'crashpad_qemu_' +
        ''.join(chr(random.randint(ord('A'), ord('Z'))) for x in range(8)))

    # These arguments are from the Fuchsia repo in zircon/scripts/run-zircon.

    # yapf: disable
    popen = subprocess.Popen([
        qemu_path,
        '-m', '2048',
        '-nographic',
        '-kernel', kernel_path,
        '-initrd', initrd_path,
        '-smp', '4',
        '-serial', 'stdio',
        '-monitor', 'none',
        '-machine', 'q35',
        '-cpu', 'host,migratable=no,+invtsc',
        '-enable-kvm',
        '-netdev', 'type=tap,ifname=qemu,script=no,downscript=no,id=net0',
        '-device', 'e1000,netdev=net0,mac=52:54:00:' + mac_tail,
        '-append', 'TERM=dumb zircon.nodename=' + instance_name,
    ],
                             stdin=DEVNULL,
                             stdout=DEVNULL,
                             stderr=DEVNULL)
    # yapf: enable

    with open(pid_file, 'wb') as f:
        f.write('%d\n' % popen.pid)

    for i in range(10):
        netaddr_path = os.path.join(fuchsia_dir, 'sdk', arch, 'tools',
                                    'netaddr')
        if subprocess.call([netaddr_path, '--nowait', instance_name],
                           stdout=open(os.devnull),
                           stderr=open(os.devnull)) == 0:
            break
        time.sleep(.5)
    else:
        print('instance did not respond after start', file=sys.stderr)
        return 1

    return 0


def main(args):
    if len(args) != 1 or args[0] not in ('start', 'stop'):
        print('usage: run_fuchsia_qemu.py start|stop', file=sys.stderr)
        return 1

    command = args[0]

    pid_file = os.path.join(tempfile.gettempdir(), 'crashpad_fuchsia_qemu_pid')
    _Stop(pid_file)
    if command == 'start':
        return _Start(pid_file)

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
