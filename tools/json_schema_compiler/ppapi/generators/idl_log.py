# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Error and information logging for IDL """

import sys


class IDLLog(object):
  """Captures and routes logging output.

  Caputres logging output and/or sends out via a file handle, typically
  stdout or stderr.
  """
  def __init__(self, name, out):
    if name:
      self._name = '%s : ' % name
    else:
      self._name = ''

    self._out = out
    self._capture = False
    self._console = True
    self._log = []

  def Log(self, msg):
    if self._console:
      line = "%s\n" % (msg)
      self._out.write(line)
    if self._capture:
      self._log.append(msg)

  def LogLine(self, filename, lineno, pos, msg):
    if self._console:
      line = "%s(%d) : %s%s\n" % (filename, lineno, self._name, msg)
      self._out.write(line)
    if self._capture:
      self._log.append(msg)

  def SetConsole(self, enable):
    self._console = enable

  def SetCapture(self, enable):
    self._capture = enable

  def DrainLog(self):
    out = self._log
    self._log = []
    return out

ErrOut  = IDLLog('Error', sys.stderr)
WarnOut = IDLLog('Warning', sys.stdout)
InfoOut = IDLLog('', sys.stdout)
