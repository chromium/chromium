#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Diagnose some common system configuration problems on Linux and macOS, and
suggest fixes."""

from __future__ import print_function

import os
import subprocess
import sys
import socket
from typing import List, Optional, Callable, Tuple, Any

CheckCallable = Callable[[], Optional[str]]

all_checks: List[Tuple[str, CheckCallable]] = []


def Check(
    name: str,
    platforms: Optional[List[str]] = None
) -> Callable[[CheckCallable], CheckCallable]:
  """Decorator that defines a diagnostic check."""

  def wrap(func):
    if not platforms or sys.platform in platforms:
      all_checks.append((name, func))
    return func

  return wrap


@Check('/usr/bin/ld is not gold', platforms=['linux'])
def CheckSystemLd() -> Optional[str]:
  proc = subprocess.Popen(['/usr/bin/ld', '-v'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if 'GNU gold' in stdout:
    return ('When /usr/bin/ld is gold, system updates can silently\n'
            'corrupt your graphics drivers.\n'
            'Try \'sudo apt-get remove binutils-gold\'.\n')
  return None


@Check('random lds are not in the $PATH', platforms=['linux'])
def CheckPathLd() -> Optional[str]:
  proc = subprocess.Popen(['which', '-a', 'ld'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  instances = stdout.split()
  if len(instances) > 1:
    return ('You have multiple \'ld\' binaries in your $PATH:\n' +
            '\n'.join(' - ' + i for i in instances) + '\n'
            'You should delete all of them but your system one.\n'
            'gold is hooked into your build via depot tools.\n')
  return None


@Check('/usr/bin/ld doesn\'t point to gold', platforms=['linux'])
def CheckLocalGold() -> Optional[str]:
  # Check /usr/bin/ld* symlinks.
  for path in ('ld.bfd', 'ld'):
    path = '/usr/bin/' + path
    try:
      target = os.readlink(path)
    except OSError as e:
      if e.errno == 2:
        continue  # No such file
      if e.errno == 22:
        continue  # Not a symlink
      raise
    if '/usr/local/gold' in target:
      return ('%s is a symlink into /usr/local/gold.\n'
              'It\'s difficult to make a recommendation, because you\n'
              'probably set this up yourself.  But you should make\n'
              '/usr/bin/ld be the standard linker, which you likely\n'
              'renamed /usr/bin/ld.bfd or something like that.\n' % path)

  return None


@Check('random ninja binaries are not in the $PATH')
def CheckPathNinja() -> Optional[str]:
  proc = subprocess.Popen(['which', 'ninja'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if not 'depot_tools' in stdout:
    return ('The ninja binary in your path isn\'t from depot_tools:\n' +
            '    ' + stdout +
            'Remove custom ninjas from your path so that the one\n'
            'in depot_tools is used.\n')
  return None


@Check('build dependencies are satisfied', platforms=['linux'])
def CheckBuildDeps() -> Optional[str]:
  script_path = os.path.join(
      os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build',
      'install-build-deps.sh')
  proc = subprocess.Popen([script_path, '--quick-check'],
                          stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if 'WARNING' in stdout:
    return ('Your build dependencies are out-of-date.\n'
            'Run \'' + script_path + '\' to update.')
  return None


@Check('can connect to Google Storage')
def CheckNetwork() -> Optional[str]:
  # Try both IPv4 and IPv6 to detect broken dual-stack configs.
  results = []
  for family in (socket.AF_INET, socket.AF_INET6):
    try:
      s = socket.socket(family, socket.SOCK_STREAM)
      s.settimeout(2)
      s.connect(('storage.googleapis.com', 443))
      s.close()
      results.append(True)
    except (OSError, socket.timeout):
      results.append(False)

  if not any(results):
    return ('Total network failure: Cannot connect to storage.googleapis.com '
            'via IPv4 or IPv6.\n'
            'Check your internet connection, proxy settings, or firewall.')

  if False in results:
    family_name = 'IPv6' if not results[1] else 'IPv4'
    return ('Partial network failure: %s connection to storage.googleapis.com '
            'timed out.\n'
            'This can cause gclient sync and gsutil to hang while they wait '
            'for timeouts.\n'
            'Check your network routing, try disabling %s, or set '
            'G_PREFER_IPV4=1.' % (family_name, family_name))
  return None


def RunChecks() -> None:
  for name, check in all_checks:
    sys.stdout.write('* Checking %s: ' % name)
    sys.stdout.flush()
    error = check()
    if not error:
      print('ok')
    else:
      print('FAIL')
      print(error)


if __name__ == '__main__':
  RunChecks()
