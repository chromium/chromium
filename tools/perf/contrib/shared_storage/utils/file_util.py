# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import shutil
import string

_SHARED_STORAGE_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))

_PROCESSOR = os.path.abspath(
    os.path.join(_SHARED_STORAGE_DIR, os.pardir, os.pardir,
                 'results_processor'))

_HISTOGRAMS_RAW = os.path.abspath(
    os.path.join(_SHARED_STORAGE_DIR, os.pardir, os.pardir, 'histograms.json'))

_SHARED_STORAGE_FILE = os.path.abspath(
    os.path.join(_SHARED_STORAGE_DIR, 'shared_storage.py'))

_DATA_DIR = os.path.abspath(os.path.join(_SHARED_STORAGE_DIR, 'data'))

_RUN_PATH_FILE = os.path.abspath(os.path.join(_DATA_DIR, 'run_path.txt'))

_HISTOGRAMS_EXPECTED = os.path.abspath(
    os.path.join(_DATA_DIR, 'histograms_expected.json'))

_HISTOGRAMS_INFO = os.path.abspath(
    os.path.join(_DATA_DIR, 'histograms_info.json'))

_HISTOGRAMS_AGG = os.path.abspath(
    os.path.join(_DATA_DIR, 'histograms_aggregated.json'))

_HISTOGRAMS_STORY = os.path.abspath(
    os.path.join(_DATA_DIR, 'histograms_by_story.json'))

_HISTOGRAM_COUNTS = os.path.abspath(
    os.path.join(_DATA_DIR, 'histogram_counts.json'))

_HISTOGRAM_COUNT_DELTAS = os.path.abspath(
    os.path.join(_DATA_DIR, 'histogram_count_deltas.json'))

_PROCESSED_FILES = [
    _HISTOGRAMS_INFO,
    _HISTOGRAMS_AGG,
    _HISTOGRAMS_STORY,
    _HISTOGRAM_COUNTS,
    _HISTOGRAM_COUNT_DELTAS,
]


def GetProcessor():
  return _PROCESSOR


def GetRawHistogramsFile():
  return _HISTOGRAMS_RAW


def GetDataDir():
  return _DATA_DIR


def GetRunPathFile():
  return _RUN_PATH_FILE


def GetExpectedHistogramsFile():
  return _HISTOGRAMS_EXPECTED


def GetHistogramCountDeltasFile():
  return _HISTOGRAM_COUNT_DELTAS


def GetProcessedFiles():
  return _PROCESSED_FILES


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


def _MovePreviousRawHistogramsFile():
  # If histograms.json exists from a previous run, move it.
  destination = os.path.abspath(os.path.join(_DATA_DIR, 'histograms_.json'))
  _MovePreviousFile(_HISTOGRAMS_RAW, dest_path_without_time=destination)


def _MovePreviousProcessedHistogramsFiles():
  for filename in _PROCESSED_FILES:
    _MovePreviousFile(filename)


def MovePreviousHistogramsFiles():
  _MovePreviousRawHistogramsFile()
  _MovePreviousProcessedHistogramsFiles()


def GetExpectedHistogramsDictionary():
  counts_data = {}
  with open(_HISTOGRAMS_EXPECTED, 'r') as f:
    counts_data = json.load(f)
  return counts_data


def GetBenchmarkDBSize(benchmark_name):
  # We read the file instead of importing to prevent a circular import.
  benchmark_file = None
  if (not os.path.exists(_SHARED_STORAGE_FILE)
      or not os.path.isfile(_SHARED_STORAGE_FILE)):
    raise RuntimeError('%s does not exist or is not a file' %
                       _SHARED_STORAGE_FILE)
  with open(_SHARED_STORAGE_FILE, 'r') as f:
    benchmark_file = f.read()

  name_pos = benchmark_file.find(benchmark_name)
  if name_pos < 0:
    raise ValueError('benchmark_name %s not found' % benchmark_name)
  size_prefix = 'SIZE = '
  size_prefix_pos = benchmark_file.rfind(size_prefix, 0, name_pos - 1)
  if size_prefix_pos < 0:
    raise RuntimeError("%s not found before benchmark name" % size_prefix)
  start_pos = size_prefix_pos + len(size_prefix)
  end_pos = start_pos + 1
  while end_pos < name_pos and benchmark_file[end_pos] in string.digits:
    end_pos += 1
  size_str = benchmark_file[start_pos:end_pos]
  if not size_str.isdigit():
    raise RuntimeError('Expected %s to be castable to an integer' % size_str)
  return int(size_str)
