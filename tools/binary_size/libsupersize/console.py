# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""An interactive console for looking analyzing .size files."""

import argparse
import bisect
import code
import collections
import contextlib
import itertools
import logging
import os
import readline
import subprocess
import sys
import types

import archive
import canned_queries
import data_quality
import describe
import dex_disassembly
import diff
import file_format
import match_util
import models
import native_disassembly
import path_util
import readelf
import string_extract


# Number of lines before using less for Print().
_THRESHOLD_FOR_PAGER = 50

@contextlib.contextmanager
def _LessPipe():
  """Output to `less`. Yields a file object to write to."""
  try:
    # pylint: disable=unexpected-keyword-arg
    proc = subprocess.Popen(['less'],
                            stdin=subprocess.PIPE,
                            stdout=sys.stdout,
                            encoding='utf-8')
    yield proc.stdin
    proc.stdin.close()
    proc.wait()
  except IOError:
    pass  # Happens when less is quit before all data is written.
  except KeyboardInterrupt:
    pass  # Assume used to break out of less.


def _WriteToStream(lines, has_newlines=False, use_pager=None, to_file=None):
  if to_file:
    use_pager = False
  if use_pager is None and sys.stdout.isatty():
    # Does not take into account line-wrapping... Oh well.
    first_lines = list(itertools.islice(lines, _THRESHOLD_FOR_PAGER))
    use_pager = len(first_lines) == _THRESHOLD_FOR_PAGER
    lines = itertools.chain(first_lines, lines)

  if has_newlines:
    func = lambda lines, obj: obj.writelines(lines)
  else:
    func = lambda lines, obj: describe.WriteLines(lines, obj.write)

  if use_pager:
    with _LessPipe() as stdin:
      func(lines, stdin)
  elif to_file:
    with open(to_file, 'w') as file_obj:
      func(lines, file_obj)
  else:
    func(lines, sys.stdout)


@contextlib.contextmanager
def _ReadlineSession():
  history_file = os.path.join(os.path.expanduser('~'),
                              '.binary_size_query_history')
  # Without initializing readline, arrow keys don't even work!
  readline.parse_and_bind('tab: complete')
  if os.path.exists(history_file):
    readline.read_history_file(history_file)
  yield
  readline.write_history_file(history_file)


class _Session:

  def __init__(self, size_infos, output_directory_finder):
    self._printed_variables = []
    self._variables = {
        'Print': self._PrintFunc,
        'CheckDataQuality': self._CheckDataQuality,
        'Csv': self._CsvFunc,
        'Diff': self._DiffFunc,
        'SaveSizeInfo': self._SaveSizeInfo,
        'SaveDeltaSizeInfo': self._SaveDeltaSizeInfo,
        'ReadStringLiterals': self._ReadStringLiterals,
        'ReplaceWithRelocations': self._ReplaceWithRelocations,
        'Disassemble': self._DisassembleFunc,
        'ExpandRegex': match_util.ExpandRegexIdentifierPlaceholder,
        'SizeStats': self._SizeStats,
        'ShowExamples': self._ShowExamplesFunc,
        'canned_queries': canned_queries.CannedQueries(size_infos),
        'printed': self._printed_variables,
        'models': models,
    }
    self._output_directory_finder = output_directory_finder
    self._size_infos = size_infos
    self._dex_disassembly_cache_by_size_info = collections.defaultdict(
        dex_disassembly.CreateCache)

    if len(size_infos) == 1:
      self._variables['size_info'] = size_infos[0]
    else:
      for i, size_info in enumerate(size_infos):
        self._variables['size_info%d' % (i + 1)] = size_info

  def _ReadStringLiterals(self, thing=None, all_rodata=False, elf_path=None):
    """Returns a list of (symbol, string value) for all string literal symbols.

    E.g.:
      # Print sorted list of all string literals:
      Print(sorted(x[1] for x in ReadStringLiterals()))
    Args:
      thing: Can be a Symbol, iterable of symbols, or SizeInfo.
           Defaults to the current SizeInfo.
      all_rodata: Assume every symbol within .rodata that ends in a \0 is a
           string literal.
      elf_path: Path to the executable containing the symbol. Required only
          when auto-detection fails.
    """
    if thing is None:
      thing = self._size_infos[-1]
    if isinstance(thing, models.SizeInfo):
      thing = thing.raw_symbols.IterUniqueSymbols()
    elif isinstance(thing, models.BaseSymbol):
      thing = thing.IterLeafSymbols()

    thing, thing_clone = itertools.tee(thing)
    first_sym = next(thing_clone, None)
    if not first_sym:
      return []
    size_info = self._SizeInfoForSymbol(first_sym)
    container = first_sym.container
    elf_path = self._ElfPathForSymbol(size_info, container, elf_path)

    return string_extract.ReadStringLiterals(thing,
                                             elf_path,
                                             all_rodata=all_rodata)

  def _DiffFunc(self, before=None, after=None, sort=True):
    """Diffs two SizeInfo objects. Returns a DeltaSizeInfo.

    Args:
      before: Defaults to size_infos[0].
      after: Defaults to size_infos[1].
      sort: When True (default), calls SymbolGroup.Sorted() after diffing.
    """
    before = before if before is not None else self._size_infos[0]
    after = after if after is not None else self._size_infos[1]
    return diff.Diff(before, after, sort=sort)

  def _PrintUploadCommand(self, file_to_upload, is_internal=False):
    oneoffs_dir = 'oneoffs'
    visibility = '-a public-read '
    if is_internal:
      oneoffs_dir = 'private-oneoffs'
      visibility = ''

    shortname = os.path.basename(os.path.normpath(file_to_upload))
    msg = (
        'Saved locally to {local}. To share, run:\n'
        '> gsutil.py cp {visibility}{local} gs://chrome-supersize/'
        '{oneoffs_dir}\n'
        '  Then view it at https://chrome-supersize.firebaseapp.com/viewer.html'
        '?load_url=https://storage.googleapis.com/chrome-supersize/'
        '{oneoffs_dir}/{shortname}')
    print(msg.format(local=file_to_upload,
                     shortname=shortname,
                     oneoffs_dir=oneoffs_dir,
                     visibility=visibility))

  def _SaveSizeInfo(self, size_info=None, to_file=None):
    """Writes a .size file.

    Args:
      size_info: The size_info to filter. Defaults to size_infos[0].
      to_file: Defaults to default.size
    """
    size_info = size_info or self._size_infos[0]
    to_file = to_file or 'default.size'
    assert to_file.endswith('.size'), 'to_file should end with .size'

    file_format.SaveSizeInfo(size_info, to_file)

    is_internal = len(size_info.symbols.WherePathMatches('^clank')) > 0
    self._PrintUploadCommand(to_file, is_internal)

  def _SaveDeltaSizeInfo(self, size_info, to_file=None):
    """Writes a .sizediff file.

    Args:
      delta_size_info: The delta_size_info to filter.
      to_file: Defaults to default.sizediff
    """
    to_file = to_file or 'default.sizediff'
    assert to_file.endswith('.sizediff'), 'to_file should end with .sizediff'

    file_format.SaveDeltaSizeInfo(size_info, to_file)
    is_internal = len(size_info.symbols.WherePathMatches('^clank')) > 0
    self._PrintUploadCommand(to_file, is_internal)

  def _SizeStats(self, size_info=None):
    """Prints some statistics for the given size info.

    Args:
      size_info: Defaults to size_infos[0].
    """
    size_info = size_info or self._size_infos[0]
    describe.WriteLines(data_quality.DescribeSizeInfoCoverage(size_info),
                        sys.stdout.write)

  def _CheckDataQuality(self, size_info=None, track_string_literals=True):
    """Performs checks that run as part of --check-data-quality."""
    size_info = size_info or self._size_infos[0]
    data_quality.CheckDataQuality(size_info, track_string_literals)

  def _PrintFunc(self, obj=None, verbose=False, summarize=True, recursive=False,
                 use_pager=None, to_file=None):
    """Prints out the given Symbol / SymbolGroup / SizeInfo.

    For convenience, |obj| will be appended to the global "printed" list.

    Args:
      obj: The object to be printed.
      verbose: Show more detailed output.
      summarize: If False, show symbols only (no headers / summaries).
      recursive: Print children of nested SymbolGroups.
      use_pager: Pipe output through `less`. Ignored when |obj| is a Symbol.
          default is to automatically pipe when output is long.
      to_file: Rather than print to stdio, write to the given file.
    """
    if obj is not None:
      self._printed_variables.append(obj)
    lines = describe.GenerateLines(
        obj, verbose=verbose, recursive=recursive, summarize=summarize,
        format_name='text')
    _WriteToStream(lines, use_pager=use_pager, to_file=to_file)

  def _CsvFunc(self, obj=None, verbose=False, use_pager=None, to_file=None):
    """Prints out the given Symbol / SymbolGroup / SizeInfo in CSV format.

    For convenience, |obj| will be appended to the global "printed" list.

    Args:
      obj: The object to be printed as CSV.
      use_pager: Pipe output through `less`. Ignored when |obj| is a Symbol.
          default is to automatically pipe when output is long.
      to_file: Rather than print to stdio, write to the given file.
    """
    if obj is not None:
      self._printed_variables.append(obj)
    lines = describe.GenerateLines(obj, verbose=verbose, recursive=False,
                                   format_name='csv')
    _WriteToStream(lines, use_pager=use_pager, to_file=to_file)

  def _ElfPathForSymbol(self, size_info, container, elf_path=None):
    def build_id_matches(elf_path):
      found_build_id = readelf.BuildIdFromElf(elf_path)
      expected_build_id = container.metadata.get(models.METADATA_ELF_BUILD_ID)
      return found_build_id == expected_build_id

    filename = container.metadata[models.METADATA_ELF_FILENAME]
    paths_to_try = []
    if elf_path:
      paths_to_try.append(elf_path)
    else:
      auto_output_directory_finders = [
          path_util.OutputDirectoryFinder(
              any_path_within_output_directory=s.size_path)
          for s in self._size_infos] + [self._output_directory_finder]
      for output_directory_finder in auto_output_directory_finders:
        output_dir = output_directory_finder.Tentative()
        if output_dir:
          # Local build: File is located in output directory.
          paths_to_try.append(
              os.path.normpath(os.path.join(output_dir, filename)))
        # Downloaded build: File is located beside .size file.
        paths_to_try.append(os.path.normpath(os.path.join(
            os.path.dirname(size_info.size_path), os.path.basename(filename))))

    paths_to_try = [p for p in paths_to_try if os.path.exists(p)]

    for i, path in enumerate(paths_to_try):
      if build_id_matches(path):
        return path

      # Show an error only once all paths are tried.
      if i + 1 == len(paths_to_try):
        assert False, 'Build ID does not match for %s' % elf_path

    assert False, (
        'Could not locate ELF file. If binary was built locally, ensure '
        '--output-directory is set. If output directory is unavailable, '
        'ensure {} is located beside {}, or pass its path explicitly using '
        'elf_path=').format(os.path.basename(filename), size_info.size_path)
    return None

  def _SizeInfoForSymbol(self, symbol):
    for size_info in self._size_infos:
      if symbol in size_info.raw_symbols:
        return size_info
    assert False, 'Symbol does not belong to a size_info.'
    return None

  def _DisassembleNative(self,
                         symbol,
                         output_directory,
                         elf_path=None,
                         use_pager=None,
                         to_file=None):
    if output_directory is None:
      # If we do not know/guess the output directory, run from any directory 2
      # levels below src since it is better than a random cwd (because usually
      # source file paths are relative to an output directory two levels below
      # src and start with ../../).
      output_directory = path_util.FromToolsSrcRoot('tools', 'binary_size')

    size_info = self._SizeInfoForSymbol(symbol)
    elf_path = self._ElfPathForSymbol(size_info, symbol.container, elf_path)

    with native_disassembly.Disassemble(symbol,
                                        output_directory,
                                        elf_path,
                                        max_bytes=None) as lines:
      _WriteToStream(lines,
                     has_newlines=True,
                     use_pager=use_pager,
                     to_file=to_file)

  def _DisassembleDex(self,
                      symbol,
                      output_directory,
                      use_pager=None,
                      to_file=None):

    def path_resolver(path):
      return os.path.join(output_directory, path)

    size_info = self._SizeInfoForSymbol(symbol)
    cache = self._dex_disassembly_cache_by_size_info[size_info]
    lines = dex_disassembly.Disassemble(symbol, path_resolver, cache)
    _WriteToStream(iter(lines),
                   has_newlines=True,
                   use_pager=use_pager,
                   to_file=to_file)

  def _DisassembleFunc(self,
                       symbol,
                       elf_or_apk_path=None,
                       use_pager=None,
                       to_file=None):
    """Shows objdump disassembly for the given symbol.

    Args:
      symbol: Must be a .text symbol and not a SymbolGroup.
      elf_or_apk_path: Path to the executable containing the symbol. Required
          only when auto-detection fails.
    """
    assert not symbol.IsGroup()
    assert not symbol.IsDelta(), ('Cannot disasseble a Diff\'ed symbol. Try '
                                  'passing .before_symbol or .after_symbol.')

    output_directory_finder = self._output_directory_finder
    if not output_directory_finder.Tentative():
      output_directory_finder = path_util.OutputDirectoryFinder(
          any_path_within_output_directory=elf_or_apk_path)
    output_directory = output_directory_finder.Tentative()

    if symbol.section_name == models.SECTION_TEXT:
      assert symbol.address, 'Symbol is missing address'
      self._DisassembleNative(symbol,
                              output_directory,
                              elf_path=elf_or_apk_path,
                              use_pager=use_pager,
                              to_file=to_file)
    elif symbol.section_name == models.SECTION_DEX_METHOD:
      self._DisassembleDex(symbol,
                           output_directory,
                           use_pager=use_pager,
                           to_file=to_file)
    else:
      raise Exception('Symbol type is not supported: ' + symbol.section_name)


  def _ReplaceWithRelocations(self, size_info=None):
    """Replace all symbol sizes with counts of native relocations.

    Removes all symbols that do not contain relocations.

    Args:
      size_info: The size_info to filter. Defaults to size_infos[0].

    Returns:
      A new SizeInfo.
    """
    size_info = size_info or self._size_infos[0]

    new_syms = []
    new_containers = []

    for container, group in itertools.groupby(
        size_info.raw_symbols, lambda s: s.container):
      if models.METADATA_ELF_FILENAME not in container.metadata:
        continue

      raw_symbols = [s for s in group if s.IsNative()]
      if not raw_symbols:
        continue

      new_containers.append(container)

      elf_path = self._ElfPathForSymbol(size_info, container)
      relro_addresses = readelf.CollectRelocationAddresses(elf_path)

      # More likely for there to be a bug in supersize than an ELF to have any
      # relative relocations.
      assert relro_addresses

      # Last symbol address is the end of the last symbol, so we don't
      # misattribute all relros after the last symbol to that symbol.
      symbol_addresses = [s.address for s in raw_symbols]
      symbol_addresses.append(raw_symbols[-1].end_address)

      for symbol in raw_symbols:
        symbol.address = 0
        symbol.size = 0
        symbol.padding = 0

      logging.info('Adding %d relocations', len(relro_addresses))
      for addr in relro_addresses:
        # Attribute relros to largest symbol start address that precede them.
        idx = bisect.bisect_right(symbol_addresses, addr) - 1
        if 0 <= idx < len(raw_symbols):
          symbol = raw_symbols[idx]
          for alias in symbol.aliases or [symbol]:
            alias.size += 1

      new_syms.extend(s for s in raw_symbols if s.size)

    return models.SizeInfo(size_info.build_config,
                           new_containers,
                           models.SymbolGroup(new_syms),
                           size_path=size_info.size_path)

  def _ShowExamplesFunc(self):
    print(self._CreateBanner())
    print('\n'.join([
        '# Show pydoc for main types:',
        'import models',
        'help(models)',
        '',
        '# Show all attributes of all symbols & per-section totals:',
        'Print(size_info, verbose=True)',
        '',
        '# Dump section info and all symbols in CSV format:',
        'Csv(size_info)',
        '',
        '# Print sorted list of all string literals:',
        'Print(sorted(x[1] for x in ReadStringLiterals()))',
        '',
        '# Show two levels of .text, grouped by first two subdirectories',
        'text_syms = size_info.symbols.WhereInSection("t")',
        'by_path = text_syms.GroupedByPath(depth=2)',
        'Print(by_path.WherePssBiggerThan(1024))',
        '',
        '# Show all generated symbols, then show only non-vtable ones',
        'Print(size_info.symbols.WhereGeneratedByToolchain())',
        'Print(printed[-1].WhereNameMatches(r"vtable").Inverted().Sorted())',
        '',
        '# Show all symbols that have "print" in their name or path, except',
        '# those within components/.',
        '# Note: Could have also used Inverted(), as above.',
        '# Note: Use "help(ExpandRegex)" for more about what {{_print_}} does.',
        'print_syms = size_info.symbols.WhereMatches(r"{{_print_}}")',
        'Print(print_syms - print_syms.WherePathMatches(r"^components/"))',
        '',
        '# Diff two .size files and save result to a file:',
        'Print(Diff(size_info1, size_info2), to_file="output.txt")',
        '',
        '# Save a .size containing only the filtered symbols',
        'filtered_symbols = size_info.raw_symbols.Filter(lambda l: l.IsPak())',
        'size_info.MakeSparse(filtered_symbols)',
        'SaveSizeInfo(size_info, to_file="oneoff_paks.size")',
        '',
        '# View per-component breakdowns, then drill into the last entry.',
        'c = canned_queries.CategorizeByChromeComponent()',
        'Print(c)',
        'Print(c[-1].GroupedByPath(depth=2).Sorted())',
        '',
        '# For even more inspiration, look at canned_queries.py',
        '# (and feel free to add your own!).',
    ]))

  def _CreateBanner(self):
    def keys(cls, super_keys=None):
      ret = sorted(m for m in dir(cls) if m[0] != '_')
      if super_keys:
        ret = sorted(m for m in ret if m not in super_keys)
      return ret

    symbol_info_keys = keys(models.SizeInfo)
    symbol_keys = keys(models.Symbol)
    symbol_group_keys = keys(models.SymbolGroup, symbol_keys)
    delta_size_info_keys = keys(models.DeltaSizeInfo)
    delta_symbol_keys = keys(models.DeltaSymbol, symbol_keys)
    delta_symbol_group_keys = keys(models.DeltaSymbolGroup,
                                   symbol_keys + symbol_group_keys)
    canned_queries_keys = keys(canned_queries.CannedQueries)

    functions = sorted(k for k in self._variables if k[0].isupper())
    lines = [
        '*' * 80,
        'Entering interactive Python shell. Quick reference:',
        '',
        'SizeInfo: %s' % ', '.join(symbol_info_keys),
        'Symbol: %s' % ', '.join(symbol_keys),
        '',
        'SymbolGroup (extends Symbol): %s' % ', '.join(symbol_group_keys),
        '',
        'DeltaSizeInfo: %s' % ', '.join(delta_size_info_keys),
        'DeltaSymbol (extends Symbol): %s' % ', '.join(delta_symbol_keys),
        'DeltaSymbolGroup (extends SymbolGroup): %s' % ', '.join(
            delta_symbol_group_keys),
        '',
        'canned_queries: %s' % ', '.join(canned_queries_keys),
        '',
        'Functions: %s' % ', '.join('%s()' % f for f in functions),
        'Variables:',
        '  printed: List of objects passed to Print().',
    ]
    for key, value in self._variables.items():
      if isinstance(value, types.ModuleType):
        continue
      if key.startswith('size_info'):
        # pylint: disable=no-member
        lines.append(f'  {key}: Loaded from {value.size_path}')
        # pylint: enable=no-member
    lines.append('*' * 80)
    return '\n'.join(lines)

  def Eval(self, query):
    exec (query, self._variables)

  def GoInteractive(self):
    with _ReadlineSession():
      code.InteractiveConsole(self._variables).interact(self._CreateBanner())


def AddArguments(parser):
  parser.add_argument(
      'inputs', nargs='+',
      help='Input .size files to load. For a single file, it will be mapped to '
           'the variable "size_info". For multiple inputs, the names will be '
           'size_info1, size_info2, etc.')
  parser.add_argument('--query',
                      help='Execute the given snippet. '
                           'Example: Print(size_info)')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory. Used only for '
                           'Disassemble().')


def Run(args, on_config_error):
  # Up-front check for faster error-checking.
  for path in args.inputs:
    if not path.endswith('.size') and not path.endswith('.sizediff'):
      on_config_error('All inputs must end with ".size" or ".sizediff"')

  size_infos = []
  for path in args.inputs:
    logging.warning('Loading %s', path)
    if path.endswith('.sizediff'):
      size_infos.extend(archive.LoadAndPostProcessDeltaSizeInfo(path))
    else:
      size_infos.append(archive.LoadAndPostProcessSizeInfo(path))
  output_directory_finder = path_util.OutputDirectoryFinder(
      value=args.output_directory,
      any_path_within_output_directory=args.inputs[0])
  session = _Session(size_infos, output_directory_finder)

  if args.query:
    logging.info('Running query from command-line.')
    session.Eval(args.query)
  else:
    logging.info('Entering interactive console.')
    session.GoInteractive()

  # Exit without running GC, which can save multiple seconds due the large
  # number of objects created. It meants atexit and __del__ calls are not
  # made, but this shouldn't matter for console.
  sys.stdout.flush()
  sys.stderr.flush()
  os._exit(0)
