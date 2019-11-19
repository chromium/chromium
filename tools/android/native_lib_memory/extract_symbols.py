#!/usr/bin/python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Maps code pages to object files.

For all pages from the native library .text section, extract all object files
the code maps to. Outputs a web-based visualization of page -> symbol mappings,
reached symbols and code residency.
"""

import argparse
import collections
import json
import logging
import multiprocessing
import os
import shutil
import SimpleHTTPServer
import SocketServer
import sys

_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

sys.path.append(os.path.join(_SRC_PATH, 'tools', 'cygprofile'))
import cyglog_to_orderfile
import symbol_extractor

_PAGE_SIZE = 1 << 12
_PAGE_MASK = ~(_PAGE_SIZE - 1)


def _GetSymbolNameToFilename(build_directory):
  """Parses object files in a directory, and maps mangled symbol names to files.

  Object files are assumed to actually be LLVM bitcode files, that is this
  assumes clang as the compiler and the use of LTO.

  Args:
    build_directory: (str) Build directory.

  Returns:
    {symbol name (str): object filename (str)}. Filenames are stripped of the
    output_directory part.
  """
  symbol_extractor.CheckLlvmNmExists()
  path = os.path.join(build_directory, 'obj')
  object_filenames = cyglog_to_orderfile.GetObjectFilenames(path)
  pool = multiprocessing.Pool()
  symbol_names_filename = zip(
      pool.map(symbol_extractor.SymbolNamesFromLlvmBitcodeFile,
               object_filenames),
      object_filenames)
  pool.close()
  result = {}
  for (symbol_names, filename) in symbol_names_filename:
    stripped_filename = filename[len(build_directory):]
    if stripped_filename.startswith('/obj/'):
      stripped_filename = stripped_filename[len('/obj/'):]
    for s in symbol_names:
      result[s] = stripped_filename
  return result


def CodePagesToMangledSymbols(symbol_infos, text_start_offset):
  """Groups a list of symbol per code page.

  Args:
    symbol_infos: (symbol_extractor.SymbolInfo) List of symbols.
    text_start_offset: (int) Offset to add to symbol offsets. This is used to
                       account for the start of .text not being at the start of
                       a page in memory.

  Returns:
    {offset: [(mangled_name, size_in_page), ...]}
  """
  # Different symbols can be at the same address, through identical code folding
  # for instance. In this case, only keep the first one. This is not ideal, as
  # file attribution will be incorrect in this case. However ICF mostly works
  # with small symbols, so it shouldn't impact numbers too much.
  result = collections.defaultdict(set)
  known_offsets = set()
  for s in symbol_infos:
    assert s.offset % 2 == 0, 'Wrong alignment'
    if s.offset in known_offsets:
      continue
    known_offsets.add(s.offset)
    start, end = (s.offset + text_start_offset,
                  (s.offset + s.size + text_start_offset))
    start_page, end_page = start & _PAGE_MASK, end & _PAGE_MASK
    page = start_page
    while page <= end_page:
      symbol_start_in_page = max(page, start)
      symbol_end_in_page = min(page + _PAGE_SIZE, end)
      size_in_page = symbol_end_in_page - symbol_start_in_page
      result[page].add((s.name, size_in_page))
      page += _PAGE_SIZE
  for page in result:
    total_size = sum(s[1] for s in result[page])
    if total_size > _PAGE_SIZE:
      logging.warning('Too many symbols in page (%d * 4k)! Total size: %d',
                      page / _PAGE_SIZE, total_size)
  return result


def ReadReachedSymbols(filename):
  """Reads a list of reached symbols from a file.

  Args:
    filename: (str) File to read.

  Returns:
    [str] List of symbol names.
  """
  with open(filename, 'r') as f:
    return [line.strip() for line in f.readlines()]


def WriteReachedData(filename, page_to_reached_data):
  """Writes the page to reached fraction to a JSON file.

  The output format is suited for visualize.html.

  Args:
    filename: (str) Output filename.
    page_to_reached_data: (dict) As returned by CodePagesToReachedSize().
  """
  json_object = []
  for (offset, data) in page_to_reached_data.items():
    json_object.append({'offset': offset, 'total': data['total'],
                        'reached': data['reached']})
  with open(filename, 'w') as f:
    json.dump(json_object, f)


def CodePagesToReachedSize(reached_symbol_names, page_to_symbols):
  """From page offset -> [all_symbols], return the reached portion per page.

  Args:
    reached_symbol_names: ([str]) List of reached symbol names.
    page_to_symbols: (dict) As returned by CodePagesToMangledSymbols().

  Returns:
    {page offset (int) -> {'total': int, 'reached': int}}
  """
  reached_symbol_names = set(reached_symbol_names)
  page_to_reached = {}
  for offset in page_to_symbols:
    total_size = sum(x[1] for x in page_to_symbols[offset])
    reached_size = sum(
        size_in_page for (name, size_in_page) in page_to_symbols[offset]
        if name in reached_symbol_names)
    page_to_reached[offset] = {'total': total_size, 'reached': reached_size}
  return page_to_reached


def CodePagesToObjectFiles(symbols_to_object_files, code_pages_to_symbols):
  """From symbols in object files and symbols in pages, gives code page to
  object files.

  Args:
    symbols_to_object_files: (dict) as returned by _GetSymbolNameToFilename()
    code_pages_to_symbols: (dict) as returned by CodePagesToMangledSymbols()

  Returns:
    {page_offset: {object_filename: size_in_page}}
  """
  result = {}
  unmatched_symbols_count = 0
  unmatched_symbols_size = 0
  for page_address in code_pages_to_symbols:
    result[page_address] = {}
    for name, size_in_page in code_pages_to_symbols[page_address]:
      if name not in symbols_to_object_files:
        unmatched_symbols_count += 1
        unmatched_symbols_size += size_in_page
        continue
      object_filename = symbols_to_object_files[name]
      if object_filename not in result[page_address]:
        result[page_address][object_filename] = 0
      result[page_address][object_filename] += size_in_page
  logging.warning('%d unmatched symbols (total size %d).',
                  unmatched_symbols_count, unmatched_symbols_size)
  return result


def WriteCodePageAttribution(page_to_object_files, text_filename,
                             json_filename):
  """Writes the code page -> file mapping in text and JSON format.

  Args:
    page_to_object_files: As returned by CodePagesToObjectFiles().
    text_filename: (str) Text output filename.
    json_filename: (str) JSON output filename.
  """
  json_data = []
  with open(text_filename, 'w') as f:
    for page_offset in sorted(page_to_object_files.keys()):
      size_and_filenames = [(kv[1], kv[0])
                            for kv in page_to_object_files[page_offset].items()]
      size_and_filenames.sort(reverse=True)
      total_size = sum(x[0] for x in size_and_filenames)
      json_data.append({'offset': page_offset, 'accounted_for': total_size,
                        'size_and_filenames': size_and_filenames})
      f.write('Page Offset: %d * 4k (accounted for: %d)\n' % (
          page_offset / (1 << 12), total_size))
      for size, filename in size_and_filenames:
        f.write('  %d\t%s\n' % (size, filename))
  with open(json_filename, 'w') as f:
    json.dump(json_data, f)


def CreateArgumentParser():
  """Creates and returns the argument parser."""
  parser = argparse.ArgumentParser(description='Map code pages to paths')
  parser.add_argument('--native-library', type=str, default='libchrome.so',
                      help=('Native Library, e.g. libchrome.so or '
                            'libmonochrome.so'))
  parser.add_argument('--reached-symbols-file', type=str,
                      help='Path to the list of reached symbols, as generated '
                      'by tools/cygprofile/process_profiles.py',
                      required=False)
  parser.add_argument('--residency', type=str,
                      help='Path to JSON file with residency pages, as written'
                      ' by extract_resident_pages.py', required=False)
  parser.add_argument('--build-directory', type=str, help='Build directory',
                      required=True)
  parser.add_argument('--output-directory', type=str, help='Output directory',
                      required=True)
  parser.add_argument('--arch', type=str, help='Architecture', default='arm')
  parser.add_argument('--start-server', action='store_true', default=False,
                      help='Run an HTTP server in the output directory')
  parser.add_argument('--port', type=int, default=8000,
                      help='Port to use for the HTTP server.')
  return parser


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.basicConfig(level=logging.INFO)

  symbol_extractor.SetArchitecture(args.arch)
  logging.info('Parsing object files in %s', args.build_directory)
  object_files_symbols = _GetSymbolNameToFilename(args.build_directory)
  native_lib_filename = os.path.join(
      args.build_directory, 'lib.unstripped', args.native_library)
  if not os.path.exists(native_lib_filename):
    logging.error('Native library not found. Did you build the APK?')
    return 1

  offset = 0
  if args.residency:
    if not os.path.exists(args.residency):
      logging.error('Residency file not found')
      return 1
    residency_path = os.path.join(args.output_directory, 'residency.json')
    if residency_path  != args.residency:
      shutil.copy(args.residency, residency_path)

  logging.info('Extracting symbols from %s', native_lib_filename)
  native_lib_symbols = symbol_extractor.SymbolInfosFromBinary(
      native_lib_filename)
  logging.info('%d Symbols found', len(native_lib_symbols))
  logging.info('Mapping symbols and object files to code pages')
  page_to_symbols = CodePagesToMangledSymbols(native_lib_symbols, offset)
  page_to_object_files = CodePagesToObjectFiles(object_files_symbols,
                                                page_to_symbols)

  if args.reached_symbols_file:
    logging.info('Mapping reached symbols to code pages')
    reached_symbol_names = ReadReachedSymbols(args.reached_symbols_file)
    reached_data = CodePagesToReachedSize(reached_symbol_names, page_to_symbols)
    WriteReachedData(os.path.join(args.output_directory, 'reached.json'),
                     reached_data)

  if not os.path.exists(args.output_directory):
    os.makedirs(args.output_directory)
  text_output_filename = os.path.join(args.output_directory, 'map.txt')
  json_output_filename = os.path.join(args.output_directory, 'map.json')
  WriteCodePageAttribution(
      page_to_object_files, text_output_filename, json_output_filename)
  directory = os.path.dirname(__file__)

  for filename in ['visualize.html', 'visualize.js', 'visualize.css']:
    shutil.copy(os.path.join(directory, filename),
                os.path.join(args.output_directory, filename))

  if args.start_server:
    os.chdir(args.output_directory)
    httpd = SocketServer.TCPServer(
        ('', args.port), SimpleHTTPServer.SimpleHTTPRequestHandler)
    logging.warning('Serving on port %d', args.port)
    httpd.serve_forever()

  return 0


if __name__ == '__main__':
  sys.exit(main())
