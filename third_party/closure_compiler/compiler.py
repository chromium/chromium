#!/usr/bin/python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Closure compiler on JavaScript files to check for errors and produce
minified output."""

from __future__ import print_function

import os
import subprocess


_CURRENT_DIR = os.path.join(os.path.dirname(__file__))
_JAVA_PATH = os.path.join(_CURRENT_DIR, "..", "jdk", "current", "bin", "java")
assert os.path.isfile(_JAVA_PATH), "java only allowed in android builds"

class Compiler(object):
  """Runs the Closure compiler on given source files to typecheck them
  and produce minified output."""

  _JAR_COMMAND = [
      _JAVA_PATH,
      "-jar",
      "-Xms1024m",
      "-client",
      "-XX:+TieredCompilation",
  ]

  def __init__(self, verbose=False):
    """
    Args:
      verbose: Whether this class should output diagnostic messages.
    """
    self._compiler_jar = os.path.join(_CURRENT_DIR, "compiler", "compiler.jar")
    self._verbose = verbose

  def _log_debug(self, msg, error=False):
    """Logs |msg| to stdout if --verbose/-v is passed when invoking this script.

    Args:
      msg: A debug message to log.
    """
    if self._verbose:
      print("(INFO) %s" % msg)

  def run_jar(self, jar, args):
    """Runs a .jar from the command line with arguments.

    Args:
      jar: A file path to a .jar file
      args: A list of command line arguments to be passed when running the .jar.

    Return:
      (exit_code, stderr) The exit code of the command (e.g. 0 for success) and
          the stderr collected while running |jar| (as a string).
    """
    shell_command = " ".join(self._JAR_COMMAND + [jar] + args)
    self._log_debug("Running jar: %s" % shell_command)

    devnull = open(os.devnull, "w")
    process = subprocess.Popen(shell_command, universal_newlines=True,
                               shell=True, stdout=devnull,
                               stderr=subprocess.PIPE)
    _, stderr = process.communicate()
    return process.returncode, stderr
