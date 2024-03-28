#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Analyzer for Object Files.

This file works around Python's lack of concurrency.

_BulkObjectFileAnalyzerWorker:
  Performs the actual work. Uses Process Pools to shard out per-object-file
  work and then aggregates results.

_BulkObjectFileAnalyzerHost:
  Creates a subprocess and sends IPCs to it asking it to do work.

_BulkObjectFileAnalyzerDelegate:
  Receives IPCs and delegates logic to _BulkObjectFileAnalyzerWorker.
  Runs _BulkObjectFileAnalyzerWorker on a background thread in order to stay
  responsive to IPCs.

BulkObjectFileAnalyzer:
  Extracts information from .o files. Alias for _BulkObjectFileAnalyzerHost,
  but when SUPERSIZE_DISABLE_ASYNC=1, alias for _BulkObjectFileAnalyzerWorker.
  * AnalyzePaths(): Processes all .o files to collect symbol names that exist
    within each. Does not work with thin archives (expand them first).
  * SortPaths(): Sort results of AnalyzePaths().
  * AnalyzeStringLiterals(): Must be run after AnalyzePaths() has completed.
    Extracts string literals from .o files, and then locates them within the
    "** merge strings" sections within an ELF's .rodata section.
  * GetSymbolNames(): Accessor.
  * Close(): Disposes data.

This file can also be run stand-alone in order to test out the logic on smaller
sample sizes.
"""

import argparse
import atexit
import collections
import errno
import logging
import os
import multiprocessing
import queue
import signal
import sys
import threading
import traceback

import bcanalyzer
import demangle
import nm
import parallel
import string_extract


_MSG_ANALYZE_PATHS = 1
_MSG_SORT_PATHS = 2
_MSG_ANALYZE_STRINGS = 3
_MSG_GET_SYMBOL_NAMES = 4
_MSG_GET_STRINGS = 5

_active_pids = None


def _DecodePosition(x):
  # Encoded as "123:123"
  sep_idx = x.index(':')
  return (int(x[:sep_idx]), int(x[sep_idx + 1:]))


class _PathsByType:
  def __init__(self, arch, obj, bc):
    self.arch = arch
    self.obj = obj
    self.bc = bc


class _BulkObjectFileAnalyzerWorker:
  def __init__(self, output_directory, track_string_literals=True):
    self._output_directory = output_directory
    self._track_string_literals = track_string_literals
    self._list_of_encoded_elf_string_ranges_by_path = None
    self._paths_by_name = collections.defaultdict(list)
    self._encoded_string_addresses_by_path_chunks = []
    self._encoded_strings_by_path_chunks = []

  def _ClassifyPaths(self, paths):
    """Classifies |paths| (.o and .a files) by file type into separate lists.

    Returns:
      A _PathsByType instance storing classified disjoint sublists of |paths|.
    """
    arch_paths = []
    obj_paths = []
    bc_paths = []
    for path in paths:
      if path.endswith('.a') or path.endswith('.rlib'):
        # .a files are typically system libraries containing .o files that are
        # ELF files (and never BC files).
        arch_paths.append(path)
      elif bcanalyzer.IsBitcodeFile(os.path.join(self._output_directory, path)):
        # Chromium build tools create BC files with .o extension. As a result,
        # IsBitcodeFile() is needed to distinguish BC files from ELF .o files.
        bc_paths.append(path)
      else:
        obj_paths.append(path)
    return _PathsByType(arch=arch_paths, obj=obj_paths, bc=bc_paths)

  def _MakeBatches(self, paths, size=None):
    if size is None:
      # Create 1-tuples of strings.
      return [(p,) for p in paths]
    # Create 1-tuples of arrays of strings.
    return [(paths[i:i + size], ) for i in range(0, len(paths), size)]

  def _DoBulkFork(self, runner, batches):
    # Order of the jobs doesn't matter since each job owns independent paths,
    # and our output is a dict where paths are the key.
    return parallel.BulkForkAndCall(
        runner,
        batches,
        output_directory=self._output_directory)

  def _RunNm(self, paths_by_type):
    """Calls nm to get symbols and (for non-BC files) string addresses."""
    # Downstream functions rely upon .a not being grouped.
    batches = self._MakeBatches(paths_by_type.arch, None)
    # Combine object files and Bitcode files for nm.
    BATCH_SIZE = 50  # Arbitrarily chosen.
    batches.extend(
        self._MakeBatches(paths_by_type.obj + paths_by_type.bc, BATCH_SIZE))
    results = self._DoBulkFork(nm.RunNmOnIntermediates, batches)

    # Names are still mangled.
    all_paths_by_name = self._paths_by_name
    total_no_symbols = 0
    for encoded_syms, encoded_strs, num_no_symbols in results:
      total_no_symbols += num_no_symbols
      symbol_names_by_path = parallel.DecodeDictOfLists(encoded_syms)
      for path, names in symbol_names_by_path.items():
        for name in names:
          all_paths_by_name[name].append(path)

      if encoded_strs != parallel.EMPTY_ENCODED_DICT:
        self._encoded_string_addresses_by_path_chunks.append(encoded_strs)
    if total_no_symbols:
      logging.warning('nm found no symbols in %d objects.', total_no_symbols)

  def _RunLlvmBcAnalyzer(self, paths_by_type):
    """Calls llvm-bcanalyzer to extract string data (for LLD-LTO)."""
    BATCH_SIZE = 50  # Arbitrarily chosen.
    batches = self._MakeBatches(paths_by_type.bc, BATCH_SIZE)
    results = self._DoBulkFork(
        bcanalyzer.RunBcAnalyzerOnIntermediates, batches)
    for encoded_strs in results:
      if encoded_strs != parallel.EMPTY_ENCODED_DICT:
        self._encoded_strings_by_path_chunks.append(encoded_strs)

  def AnalyzePaths(self, paths):
    logging.debug('worker: AnalyzePaths() started.')
    paths_by_type = self._ClassifyPaths(paths)
    logging.info('File counts: {\'arch\': %d, \'obj\': %d, \'bc\': %d}',
                 len(paths_by_type.arch), len(paths_by_type.obj),
                 len(paths_by_type.bc))
    self._RunNm(paths_by_type)
    if self._track_string_literals:
      self._RunLlvmBcAnalyzer(paths_by_type)
    logging.debug('worker: AnalyzePaths() completed.')

  def SortPaths(self):
    # Demangle all names, which can result in some merging of lists.
    self._paths_by_name = demangle.DemangleKeysAndMergeLists(
        self._paths_by_name)
    # Sort and uniquefy.
    for key in self._paths_by_name.keys():
      self._paths_by_name[key] = sorted(set(self._paths_by_name[key]))

  def _ReadElfStringData(self, elf_path, elf_string_ranges):
    # Read string_data from elf_path, to be shared with forked processes.
    address, offset, _ = string_extract.LookupElfRodataInfo(elf_path)
    adjust = address - offset
    abs_elf_string_ranges = (
        (addr - adjust, s) for addr, s in elf_string_ranges)
    return string_extract.ReadFileChunks(elf_path, abs_elf_string_ranges)

  def _GetEncodedRangesFromStringAddresses(self, string_data):
    params = ((chunk,)
        for chunk in self._encoded_string_addresses_by_path_chunks)
    # Order of the jobs doesn't matter since each job owns independent paths,
    # and our output is a dict where paths are the key.
    results = parallel.BulkForkAndCall(
        string_extract.ResolveStringPiecesIndirect,
        params,
        string_data=string_data,
        output_directory=self._output_directory)
    return list(results)

  def _GetEncodedRangesFromStrings(self, string_data):
    params = ((chunk,) for chunk in self._encoded_strings_by_path_chunks)
    # Order of the jobs doesn't matter since each job owns independent paths,
    # and our output is a dict where paths are the key.
    results = parallel.BulkForkAndCall(
        string_extract.ResolveStringPieces, params, string_data=string_data)
    return list(results)

  def AnalyzeStringLiterals(self, elf_path, elf_string_ranges):
    logging.debug('worker: AnalyzeStringLiterals() started.')
    string_data = self._ReadElfStringData(elf_path, elf_string_ranges)

    # [source_idx][batch_idx][section_idx] -> Encoded {path: [string_ranges]}.
    encoded_ranges_sources = [
      self._GetEncodedRangesFromStringAddresses(string_data),
      self._GetEncodedRangesFromStrings(string_data),
    ]
    # [section_idx] -> {path: [string_ranges]}.
    self._list_of_encoded_elf_string_ranges_by_path = []
    # Contract [source_idx] and [batch_idx], then decode and join.
    for section_idx in range(len(elf_string_ranges)):  # Fetch result.
      t = []
      for encoded_ranges in encoded_ranges_sources:  # [source_idx].
        t.extend([b[section_idx] for b in encoded_ranges])  # [batch_idx].
      self._list_of_encoded_elf_string_ranges_by_path.append(
          parallel.JoinEncodedDictOfLists(t))
    logging.debug('worker: AnalyzeStringLiterals() completed.')

  def GetSymbolNames(self):
    return self._paths_by_name

  def GetStringPositions(self):
    return [
        parallel.DecodeDictOfLists(x, value_transform=_DecodePosition)
        for x in self._list_of_encoded_elf_string_ranges_by_path
    ]

  def GetEncodedStringPositions(self):
    return self._list_of_encoded_elf_string_ranges_by_path

  def Close(self):
    pass


def _TerminateSubprocesses():
  global _active_pids
  if _active_pids:
    for pid in _active_pids:
      os.kill(pid, signal.SIGKILL)
    _active_pids = []


class _BulkObjectFileAnalyzerHost:
  """Runs BulkObjectFileAnalyzer in a subprocess."""

  def __init__(self, output_directory, track_string_literals=True):
    self._output_directory = output_directory
    self._track_string_literals = track_string_literals
    self._child_pid = None
    self._pipe = None

  def _Spawn(self):
    global _active_pids
    parent_conn, child_conn = multiprocessing.Pipe()
    self._child_pid = os.fork()
    if self._child_pid:
      # We are the parent process.
      if _active_pids is None:
        _active_pids = []
        atexit.register(_TerminateSubprocesses)
      _active_pids.append(self._child_pid)
      self._pipe = parent_conn
    else:
      # We are the child process.
      logging.root.handlers[0].setFormatter(logging.Formatter(
          'obj_analyzer: %(levelname).1s %(relativeCreated)6d %(message)s'))
      worker_analyzer = _BulkObjectFileAnalyzerWorker(
          self._output_directory,
          track_string_literals=self._track_string_literals)
      delegate = _BulkObjectFileAnalyzerDelegate(worker_analyzer, child_conn)
      delegate.Run()

  def AnalyzePaths(self, paths):
    if self._child_pid is None:
      self._Spawn()

    logging.debug('Sending batch of %d paths to subprocess', len(paths))
    payload = '\x01'.join(paths)
    self._pipe.send((_MSG_ANALYZE_PATHS, payload))

  def SortPaths(self):
    self._pipe.send((_MSG_SORT_PATHS,))

  def AnalyzeStringLiterals(self, elf_path, elf_string_ranges):
    self._pipe.send((_MSG_ANALYZE_STRINGS, elf_path, elf_string_ranges))

  def GetSymbolNames(self):
    self._pipe.send((_MSG_GET_SYMBOL_NAMES,))
    self._pipe.recv()  # None
    logging.debug('Decoding nm results from forked process')
    encoded_paths_by_name = self._pipe.recv()
    return parallel.DecodeDictOfLists(encoded_paths_by_name)

  def GetStringPositions(self):
    self._pipe.send((_MSG_GET_STRINGS,))
    self._pipe.recv()  # None
    logging.debug('Decoding string symbol results from forked process')
    result = self._pipe.recv()
    return [
        parallel.DecodeDictOfLists(x, value_transform=_DecodePosition)
        for x in result
    ]

  def Close(self):
    self._pipe.close()
    # Child process should terminate gracefully at this point, but leave it in
    # _active_pids to be killed just in case.


class _BulkObjectFileAnalyzerDelegate:
  """The subprocess entry point."""
  def __init__(self, worker_analyzer, pipe):
    self._worker_analyzer = worker_analyzer
    self._pipe = pipe
    # Use a worker thread so that AnalyzeStringLiterals() is non-blocking. The
    # thread allows the main thread to process a call to GetSymbolNames() while
    # AnalyzeStringLiterals() is in progress.
    self._job_queue = queue.Queue()
    self._worker_thread = threading.Thread(target=self._WorkerThreadMain)
    self._allow_analyze_paths = True

  def _WorkerThreadMain(self):
    while True:
      # Handle exceptions so test failure will be explicit and not block.
      try:
        func = self._job_queue.get()
        func()
      except Exception:
        traceback.print_exc()
      self._job_queue.task_done()

  def _WaitForAnalyzePathJobs(self):
    if self._allow_analyze_paths:
      self._job_queue.join()
      self._allow_analyze_paths = False

  # Handle messages in a function outside the event loop, so local variables are
  # independent across messages, and can be bound to jobs by lambdas using
  # closures instead of functools.partial().
  def _HandleMessage(self, message):
    if message[0] == _MSG_ANALYZE_PATHS:
      assert self._allow_analyze_paths, (
          'Cannot call AnalyzePaths() after AnalyzeStringLiterals()s.')
      # Invert '\x01'.join(paths), favoring paths = [] over paths = [''] since
      # the latter is less likely to happen.
      paths = message[1].split('\x01') if message[1] else []
      self._job_queue.put(lambda: self._worker_analyzer.AnalyzePaths(paths))
    elif message[0] == _MSG_SORT_PATHS:
      assert self._allow_analyze_paths, (
          'Cannot call SortPaths() after AnalyzeStringLiterals()s.')
      self._job_queue.put(self._worker_analyzer.SortPaths)
    elif message[0] == _MSG_ANALYZE_STRINGS:
      self._WaitForAnalyzePathJobs()
      elf_path, string_positions = message[1:]
      self._job_queue.put(
          lambda: self._worker_analyzer.AnalyzeStringLiterals(
              elf_path, string_positions))
    elif message[0] == _MSG_GET_SYMBOL_NAMES:
      self._WaitForAnalyzePathJobs()
      self._pipe.send(None)
      paths_by_name = self._worker_analyzer.GetSymbolNames()
      self._pipe.send(parallel.EncodeDictOfLists(paths_by_name))
    elif message[0] == _MSG_GET_STRINGS:
      self._job_queue.join()
      # Send a None packet so that other side can measure IPC transfer time.
      self._pipe.send(None)
      self._pipe.send(self._worker_analyzer.GetEncodedStringPositions())

  def Run(self):
    try:
      self._worker_thread.start()
      while True:
        self._HandleMessage(self._pipe.recv())
    except EOFError:
      pass
    except EnvironmentError as e:
      # Parent process exited so don't log.
      if e.errno in (errno.EPIPE, errno.ECONNRESET):
        sys.exit(1)

    logging.debug('bulk subprocess finished.')
    sys.exit(0)


BulkObjectFileAnalyzer = _BulkObjectFileAnalyzerHost
if parallel.DISABLE_ASYNC:
  BulkObjectFileAnalyzer = _BulkObjectFileAnalyzerWorker


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--multiprocess', action='store_true')
  parser.add_argument('--output-directory', required=True)
  parser.add_argument('--elf-file', type=os.path.realpath)
  parser.add_argument('--show-names', action='store_true')
  parser.add_argument('--show-strings', action='store_true')
  parser.add_argument('objects', type=os.path.realpath, nargs='+')

  args = parser.parse_args()
  logging.basicConfig(level=logging.DEBUG,
                      format='%(levelname).1s %(relativeCreated)6d %(message)s')

  if args.multiprocess:
    bulk_analyzer = _BulkObjectFileAnalyzerHost(args.output_directory)
  else:
    parallel.DISABLE_ASYNC = True
    bulk_analyzer = _BulkObjectFileAnalyzerWorker(args.output_directory)

  # Pass individually to test multiple calls.
  for path in args.objects:
    bulk_analyzer.AnalyzePaths([path])
  bulk_analyzer.SortPaths()

  names_to_paths = bulk_analyzer.GetSymbolNames()
  print('Found {} names'.format(len(names_to_paths)))
  if args.show_names:
    for name, paths in names_to_paths.items():
      print('{}: {!r}'.format(name, paths))

  if args.elf_file:
    address, offset, size = string_extract.LookupElfRodataInfo(args.elf_file)
    bulk_analyzer.AnalyzeStringLiterals(args.elf_file, ((address, size),))

    positions_by_path = bulk_analyzer.GetStringPositions()[0]
    print('Found {} string literals'.format(
        sum(len(v) for v in positions_by_path.values())))
    if args.show_strings:
      logging.debug('.rodata adjust=%d', address - offset)
      for path, positions in positions_by_path.items():
        strs = string_extract.ReadFileChunks(
            args.elf_file, ((offset + addr, size) for addr, size in positions))
        print('{}: {!r}'.format(
            path, [s if len(s) < 20 else s[:20] + '...' for s in strs]))


if __name__ == '__main__':
  main()
