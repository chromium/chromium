#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Diagnose some common system configuration problems on Linux, and
suggest fixes."""

from __future__ import print_function

import os
import subprocess
import sys

all_checks = []

def Check(name):
  """Decorator that defines a diagnostic check."""

  def wrap(func):
    all_checks.append((name, func))
    return func

  return wrap


@Check("/usr/bin/ld is not gold")
def CheckSystemLd():
  proc = subprocess.Popen(['/usr/bin/ld', '-v'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if 'GNU gold' in stdout:
    return ("When /usr/bin/ld is gold, system updates can silently\n"
            "corrupt your graphics drivers.\n"
            "Try 'sudo apt-get remove binutils-gold'.\n")
  return None


@Check("random lds are not in the $PATH")
def CheckPathLd():
  proc = subprocess.Popen(['which', '-a', 'ld'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  instances = stdout.split()
  if len(instances) > 1:
    return ("You have multiple 'ld' binaries in your $PATH:\n" +
            '\n'.join(' - ' + i for i in instances) + "\n"
            "You should delete all of them but your system one.\n"
            "gold is hooked into your build via depot tools.\n")
  return None


@Check("/usr/bin/ld doesn't point to gold")
def CheckLocalGold():
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
      return ("%s is a symlink into /usr/local/gold.\n"
              "It's difficult to make a recommendation, because you\n"
              "probably set this up yourself.  But you should make\n"
              "/usr/bin/ld be the standard linker, which you likely\n"
              "renamed /usr/bin/ld.bfd or something like that.\n" % path)

  return None


@Check("random ninja binaries are not in the $PATH")
def CheckPathNinja():
  proc = subprocess.Popen(['which', 'ninja'], stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if not 'depot_tools' in stdout:
    return ("The ninja binary in your path isn't from depot_tools:\n" + "    " +
            stdout + "Remove custom ninjas from your path so that the one\n"
            "in depot_tools is used.\n")
  return None


@Check("build dependencies are satisfied")
def CheckBuildDeps():
  script_path = os.path.join(
      os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build',
      'install-build-deps.sh')
  proc = subprocess.Popen([script_path, '--quick-check'],
                          stdout=subprocess.PIPE)
  stdout = proc.communicate()[0].decode('utf-8')
  if 'WARNING' in stdout:
    return ("Your build dependencies are out-of-date.\n"
            "Run '" + script_path + "' to update.")
  return None


def RunChecks():
  for name, check in all_checks:
    sys.stdout.write("* Checking %s: " % name)
    sys.stdout.flush()
    error = check()
    if not error:
      print("ok")
    else:
      print("FAIL")
      print(error)


if __name__ == '__main__':
  RunChecks()
