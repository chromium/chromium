# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Utility functions and objects for logging.
'''

import logging
import sys

class StdoutStderrHandler(logging.Handler):
  ''' Subclass of logging.Handler which outputs to either stdout or stderr
  based on a threshold level.
  '''

  def __init__(self, threshold=logging.WARNING, err=sys.stderr, out=sys.stdout):
    ''' Args:
          threshold: below this logging level messages are sent to stdout,
            otherwise they are sent to stderr
          err: a stream object that error messages are sent to, defaults to
            sys.stderr
          out: a stream object that non-error messages are sent to, defaults to
            sys.stdout
    '''
    logging.Handler.__init__(self)
    self._err = logging.StreamHandler(err)
    self._out = logging.StreamHandler(out)
    self._threshold = threshold
    self._last_was_err = False

  def setLevel(self, lvl):
    logging.Handler.setLevel(self, lvl)
    self._err.setLevel(lvl)
    self._out.setLevel(lvl)

  def setFormatter(self, formatter):
    logging.Handler.setFormatter(self, formatter)
    self._err.setFormatter(formatter)
    self._out.setFormatter(formatter)

  def emit(self, record):
    if record.levelno < self._threshold:
      self._out.emit(record)
      self._last_was_err = False
    else:
      self._err.emit(record)
      self._last_was_err = False

  def flush(self):
    # preserve order on the flushing, the stalest stream gets flushed first
    if self._last_was_err:
      self._out.flush()
      self._err.flush()
    else:
      self._err.flush()
      self._out.flush()


FORMAT = "%(asctime)s %(filename)s [%(levelname)s] %(message)s"
DATEFMT = "%H:%M:%S"

def config_root(level=logging.INFO, threshold=logging.WARNING, format=FORMAT,
         datefmt=DATEFMT):
  ''' Configure the root logger to use a StdoutStderrHandler and some default
  formatting.
    Args:
      level: messages below this level are ignored
      threshold: below this logging level messages are sent to stdout,
        otherwise they are sent to stderr
      format: format for log messages, see logger.Format
      datefmt: format for date in log messages

  '''
  # to set the handler of the root logging object, we need to do setup
  # manually rather than using basicConfig
  root = logging.getLogger()
  root.setLevel(level)
  formatter = logging.Formatter(format, datefmt)
  handler = StdoutStderrHandler(threshold=threshold)
  handler.setLevel(level)
  handler.setFormatter(formatter)
  root.addHandler(handler)
