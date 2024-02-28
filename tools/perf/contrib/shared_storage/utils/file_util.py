# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil

_SHARED_STORAGE_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

_DATA_DIR = os.path.abspath(os.path.join(_SHARED_STORAGE_DIR, 'data'))

_RUN_PATH_FILE = os.path.abspath(os.path.join(_DATA_DIR, 'run_path.txt'))

_HISTOGRAMS_EXPECTED = os.path.abspath(
    os.path.join(_DATA_DIR, 'histograms_expected.json'))


def GetRunPathFile():
  return _RUN_PATH_FILE


def GetExpectedHistogramsFile():
  return _HISTOGRAMS_EXPECTED


def EnsureDataDir():
  # Ensure that /data exists and is a directory. (Delete and recreate
  # if it's not a directory.)
  if os.path.exists(_DATA_DIR) and not os.path.isdir(_DATA_DIR):
    logging.warning('Deleting %s' % _DATA_DIR)
    shutil.rmtree(_DATA_DIR)
  if not os.path.exists(_DATA_DIR):
    logging.info('Creating directory %s' % _DATA_DIR)
    os.makedirs(_DATA_DIR)


def CleanUpRunPathFile():
  # Cleanup any run path file from a previous run.
  if os.path.exists(_RUN_PATH_FILE) and os.path.isfile(_RUN_PATH_FILE):
    logging.info('Removing pre-existing file %s' % _RUN_PATH_FILE)
    os.remove(_RUN_PATH_FILE)
  elif os.path.exists(_RUN_PATH_FILE) and os.path.isdir(_RUN_PATH_FILE):
    logging.warning('Deleting directory %s' % _RUN_PATH_FILE)
    shutil.rmtree(_RUN_PATH_FILE)
  elif os.path.exists(_RUN_PATH_FILE):
    msg = 'Encountered existing %s ' % _RUN_PATH_FILE
    raise RuntimeError(msg + 'that is neither a file nor a directory')


def _MovePreviousFile(src_path,
                      dest_path_without_time=None,
                      new_extension='.json'):
  if not dest_path_without_time:
    dest_path_without_time = src_path
  if not os.path.exists(src_path):
    return

  if not os.path.isfile(src_path):
    raise RuntimeError('%s is not a file' % src_path)

  # Rename a file from a previous run.
  modified = os.path.getmtime(src_path)
  destination = ''.join([
      dest_path_without_time[:-len(new_extension)],
      str(modified), new_extension
  ])
  shutil.move(src_path, destination)


def MovePreviousExpectedHistogramsFile():
  # If expected histograms file exists from a previous run, move it.
  _MovePreviousFile(_HISTOGRAMS_EXPECTED)


def GetExpectedHistogramsDictionary():
  counts_data = {}
  with open(_HISTOGRAMS_EXPECTED, 'r') as f:
    counts_data = json.load(f)
  return counts_data
