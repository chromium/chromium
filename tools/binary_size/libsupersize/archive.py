# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Main Python API for analyzing binary size."""

import argparse
import collections
import dataclasses
import logging
import os
import posixpath
import re
import shlex
import subprocess
import time
import zipfile

import apk
import apkanalyzer
import archive_util
import data_quality
import describe
import dir_metadata
import file_format
import function_signature
import json_config_parser
import models
import native
import pakfile
import parallel
import path_util
import readelf
import zip_util


@dataclasses.dataclass
class NativeSpec:
  # One (or more) of apk_so_path, map_path, elf_path must be non-None.
  # Path within the .apk of the .so file. Non-None only when apk_spec is.
  apk_so_path: str = None
  # Path to linker map file (if present).
  map_path: str = None
  # Path to unstripped ELF file (if present).
  elf_path: str = None
  # Whether to create symbols for each string literal.
  track_string_literals: bool = True
  # component to use for all symbols.
  component: str = None
  # Regular expression that will match generated files.
  gen_dir_regex: str = None
  # source_path prefix to use for all symbols.
  source_path_prefix: str = None

  @property
  def algorithm(self):
    if self.map_path:
      return 'linker_map'
    if self.elf_path:
      return 'dwarf'
    return 'sections'


@dataclasses.dataclass
class PakSpec:
  # One of pak_paths or apk_pak_paths must be non-None.
  pak_paths: list = None
  apk_pak_paths: list = None
  pak_info_path: str = None


@dataclasses.dataclass
class ApkSpec:
  # Path the .apk file. Never None.
  # This is a temp file when .apks is being analyzed.
  apk_path: str
  # Path to .minimal.apks (when analyzing bundles).
  minimal_apks_path: str = None
  # Proguard mapping path.
  mapping_path: str = None
  # Name of the apk split when .apks is being analyzed.
  split_name: str = None
  # Path such as: out/Release/size-info/BaseName
  size_info_prefix: str = None
  # Whether to break down classes.dex.
  analyze_dex: bool = True
  # Dict of apk_path -> source_path, provided by json config.
  path_defaults: dict = None
  # Component to use for symbols when not specified by DIR_METADATA, provided by
  # json config.
  default_component: str = ''


def _NormalizeNames(raw_symbols):
  """Ensures that all names are formatted in a useful way.

  This includes:
    - Deriving |name| and |template_name| from |full_name|.
    - Stripping of return types (for functions).
    - Moving "vtable for" and the like to be suffixes rather than prefixes.
  """
  found_prefixes = set()
  for symbol in raw_symbols:
    full_name = symbol.full_name

    # See comment in _CalculatePadding() about when this can happen. Don't
    # process names for non-native sections.
    if symbol.IsPak():
      # full_name: "about_ui_resources.grdp: IDR_ABOUT_UI_CREDITS_HTML".
      space_idx = full_name.rindex(' ')
      name = full_name[space_idx + 1:]
      symbol.template_name = name
      symbol.name = name
    elif (full_name.startswith('**') or symbol.IsOverhead()
          or symbol.IsOther()):
      symbol.template_name = full_name
      symbol.name = full_name
    elif symbol.IsDex():
      symbol.full_name, symbol.template_name, symbol.name = (
          function_signature.ParseJava(full_name))
    elif symbol.IsStringLiteral():
      symbol.full_name = full_name
      symbol.template_name = full_name
      symbol.name = full_name
    elif symbol.IsNative():
      # Remove [clone] suffix, and set flag accordingly.
      # Search from left-to-right, as multiple [clone]s can exist.
      # Example name suffixes:
      #     [clone .part.322]  # GCC
      #     [clone .isra.322]  # GCC
      #     [clone .constprop.1064]  # GCC
      #     [clone .11064]  # clang
      # http://unix.stackexchange.com/questions/223013/function-symbol-gets-part-suffix-after-compilation
      idx = full_name.find(' [clone ')
      if idx != -1:
        full_name = full_name[:idx]
        symbol.flags |= models.FLAG_CLONE

      # Clones for C symbols.
      if symbol.section == 't':
        idx = full_name.rfind('.')
        if idx != -1 and full_name[idx + 1:].isdigit():
          new_name = full_name[:idx]
          # Generated symbols that end with .123 but are not clones.
          # Find these via:
          # size_info.symbols.WhereInSection('t').WhereIsGroup().SortedByCount()
          if new_name not in ('__tcf_0', 'startup'):
            full_name = new_name
            symbol.flags |= models.FLAG_CLONE
            # Remove .part / .isra / .constprop.
            idx = full_name.rfind('.', 0, idx)
            if idx != -1:
              full_name = full_name[:idx]

      # E.g.: vtable for FOO
      idx = full_name.find(' for ', 0, 30)
      if idx != -1:
        found_prefixes.add(full_name[:idx + 4])
        full_name = '{} [{}]'.format(full_name[idx + 5:], full_name[:idx])

      # E.g.: virtual thunk to FOO
      idx = full_name.find(' to ', 0, 30)
      if idx != -1:
        found_prefixes.add(full_name[:idx + 3])
        full_name = '{} [{}]'.format(full_name[idx + 4:], full_name[:idx])

      # Strip out return type, and split out name, template_name.
      # Function parsing also applies to non-text symbols.
      # E.g. Function statics.
      symbol.full_name, symbol.template_name, symbol.name = (
          function_signature.Parse(full_name))

      # Remove anonymous namespaces (they just harm clustering).
      symbol.template_name = symbol.template_name.replace(
          '(anonymous namespace)::', '')
      symbol.full_name = symbol.full_name.replace(
          '(anonymous namespace)::', '')
      non_anonymous_name = symbol.name.replace('(anonymous namespace)::', '')
      if symbol.name != non_anonymous_name:
        symbol.flags |= models.FLAG_ANONYMOUS
        symbol.name = non_anonymous_name

    # Allow using "is" to compare names (and should help with RAM). This applies
    # to all symbols.
    function_signature.InternSameNames(symbol)

  logging.debug('Found name prefixes of: %r', found_prefixes)


def LoadAndPostProcessSizeInfo(path, file_obj=None):
  """Returns a SizeInfo for the given |path|."""
  logging.debug('Loading results from: %s', path)
  size_info = file_format.LoadSizeInfo(path, file_obj=file_obj)
  logging.info('Normalizing symbol names')
  _NormalizeNames(size_info.raw_symbols)
  logging.info('Loaded %d symbols', len(size_info.raw_symbols))
  return size_info


def LoadAndPostProcessDeltaSizeInfo(path, file_obj=None):
  """Returns a tuple of SizeInfos for the given |path|."""
  logging.debug('Loading results from: %s', path)
  before_size_info, after_size_info = file_format.LoadDeltaSizeInfo(
      path, file_obj=file_obj)
  logging.info('Normalizing symbol names')
  _NormalizeNames(before_size_info.raw_symbols)
  _NormalizeNames(after_size_info.raw_symbols)
  logging.info('Loaded %d + %d symbols', len(before_size_info.raw_symbols),
               len(after_size_info.raw_symbols))
  return before_size_info, after_size_info


def _ListSplits(minimal_apks_path):
  ret = []
  with zipfile.ZipFile(minimal_apks_path) as z:
    for filename in z.namelist():
      # E.g.:
      # splits/base-master.apk
      # splits/base-en.apk
      # splits/vr-master.apk
      # splits/vr-en.apk
      m = re.match(r'splits/(.*)-master\.apk', filename)
      if m:
        ret.append(m.group(1))
  # Make "base" comes first since that's the main chunk of work.
  # Also so that --abi-filter detection looks at it first.
  return sorted(ret, key=lambda x: (x != 'base', x))


def CreateBuildConfig(output_directory, source_directory):
  """Creates the dict to use for SizeInfo.build_info."""
  logging.debug('Constructing build_config')
  build_config = {}
  if output_directory:
    gn_args = _ParseGnArgs(os.path.join(output_directory, 'args.gn'))
    build_config[models.BUILD_CONFIG_GN_ARGS] = gn_args

  git_rev = _DetectGitRevision(source_directory)
  if git_rev:
    build_config[models.BUILD_CONFIG_GIT_REVISION] = git_rev

  return build_config


def CreateMetadata(*, apk_spec, native_spec, output_directory):
  """Creates metadata dict.

  Returns:
    A dict of models.METADATA_* -> values. Performs "best effort" extraction
    using available data.
  """
  logging.debug('Constructing metadata')
  metadata = {}

  # Ensure all paths are relative to output directory to make them hermetic.
  if output_directory:
    shorten_path = lambda path: os.path.relpath(path, output_directory)
  else:
    # If output directory is unavailable, just store basenames.
    shorten_path = os.path.basename

  if apk_spec:
    metadata[models.METADATA_APK_SIZE] = os.path.getsize(apk_spec.apk_path)
    if apk_spec.minimal_apks_path:
      metadata[models.METADATA_APK_FILENAME] = shorten_path(
          apk_spec.minimal_apks_path)
      metadata[models.METADATA_APK_SPLIT_NAME] = apk_spec.split_name
    else:
      metadata[models.METADATA_APK_FILENAME] = shorten_path(apk_spec.apk_path)

  if native_spec:
    native.AddMetadata(metadata=metadata,
                       native_spec=native_spec,
                       shorten_path=shorten_path)

  logging.debug('Constructing metadata (done)')
  return metadata


def _CreatePakSymbols(*, pak_spec, pak_id_map, apk_spec, output_directory):
  logging.debug('Creating Pak symbols')
  section_ranges = {}
  if apk_spec:
    assert apk_spec.size_info_prefix
    # Can modify |section_ranges|.
    raw_symbols = pakfile.CreatePakSymbolsFromApk(section_ranges,
                                                  apk_spec.apk_path,
                                                  pak_spec.apk_pak_paths,
                                                  apk_spec.size_info_prefix,
                                                  pak_id_map)
  else:
    # Can modify |section_ranges|.
    raw_symbols = pakfile.CreatePakSymbolsFromFiles(section_ranges,
                                                    pak_spec.pak_paths,
                                                    pak_spec.pak_info_path,
                                                    output_directory,
                                                    pak_id_map)
  return section_ranges, raw_symbols


def _CreateDexSymbols(*, apk_spec):
  """Create dex symbols for the given apk_spec.

  Args:
    apk_spec: Instance of ApkSpec or None.

  Returns:
    A tuple of (section_ranges, raw_symbols).
  """
  logging.info('Analyzing classes.dex for %s', apk_spec.split_name
               or apk_spec.apk_path)

  with zipfile.ZipFile(apk_spec.apk_path) as z:
    dex_total_size = sum(i.file_size for i in z.infolist()
                         if i.filename.endswith('.dex'))

  raw_symbols = apkanalyzer.CreateDexSymbols(apk_spec.apk_path,
                                             apk_spec.mapping_path,
                                             apk_spec.size_info_prefix,
                                             dex_total_size)
  sizes = collections.Counter()
  for s in raw_symbols:
    sizes[s.section_name] += s.pss
  assert len(sizes) <= 2, 'Unexpected: ' + str(sizes)
  dex_method_size = round(sizes[models.SECTION_DEX_METHOD])
  dex_other_size = round(sizes[models.SECTION_DEX])

  unattributed_dex = dex_total_size - dex_method_size - dex_other_size
  # Compare against -5 instead of 0 to guard against round-off errors.
  assert unattributed_dex >= -5, (
      'sum(dex_symbols.size) > filesize(classes.dex). {} vs {}'.format(
          dex_method_size + dex_other_size, dex_total_size))

  if unattributed_dex > 0:
    raw_symbols.append(
        models.Symbol(
            models.SECTION_DEX,
            unattributed_dex,
            full_name='** .dex (unattributed - includes string literals)'))

  # We can't meaningfully track section size of dex methods vs other, so
  # just fake the size of dex methods as the sum of symbols, and make
  # "dex other" responsible for any unattributed bytes.
  section_ranges = {
      models.SECTION_DEX_METHOD: (0, dex_method_size),
      models.SECTION_DEX: (0, dex_total_size - dex_method_size),
  }

  return section_ranges, raw_symbols


def CreateContainerSymbols(*, container_name, metadata, apk_spec, pak_spec,
                           native_spec, source_directory, output_directory,
                           resources_pathmap_path, pak_id_map):
  raw_symbols = []
  section_sizes = {}
  default_component = apk_spec.default_component if apk_spec else ''

  def add_syms(section_ranges,
               new_raw_symbols,
               source_path_prefix=None,
               component=None,
               paths_already_normalized=False):
    new_section_sizes = {
        k: size
        for k, (address, size) in section_ranges.items()
    }
    if models.SECTION_OTHER in new_section_sizes:
      section_sizes[models.SECTION_OTHER] = section_sizes.get(
          models.SECTION_OTHER, 0) + new_section_sizes[models.SECTION_OTHER]
      del new_section_sizes[models.SECTION_OTHER]

    assert not (set(section_sizes) & set(new_section_sizes)), (
        'Section collision: {}\n\n {}'.format(section_sizes, new_section_sizes))
    section_sizes.update(new_section_sizes)

    # E.g.: native.CreateSymbols() already calls NormalizePaths().
    if not paths_already_normalized:
      archive_util.NormalizePaths(new_raw_symbols)

    if source_path_prefix:
      # Prefix the source_path for all symbols that have a source_path assigned,
      # and that don't have it set to $APK or $GOOGLE3.
      for s in new_raw_symbols:
        if s.source_path and s.source_path[0] != '$':
          s.source_path = source_path_prefix + s.source_path

    if component is not None:
      for s in new_raw_symbols:
        s.component = component
    else:
      dir_metadata.PopulateComponents(new_raw_symbols,
                                      source_directory,
                                      default_component=default_component)
    raw_symbols.extend(new_raw_symbols)

  if native_spec:
    section_ranges, new_raw_symbols = native.CreateSymbols(
        metadata=metadata,
        apk_spec=apk_spec,
        native_spec=native_spec,
        output_directory=output_directory,
        pak_id_map=pak_id_map)
    add_syms(section_ranges,
             new_raw_symbols,
             source_path_prefix=native_spec.source_path_prefix,
             component=native_spec.component,
             paths_already_normalized=True)

  if pak_spec:
    add_syms(*_CreatePakSymbols(pak_spec=pak_spec,
                                pak_id_map=pak_id_map,
                                apk_spec=apk_spec,
                                output_directory=output_directory))
  if apk_spec:
    if apk_spec.analyze_dex:
      add_syms(*_CreateDexSymbols(apk_spec=apk_spec))
    add_syms(*apk.CreateApkOtherSymbols(
        metadata=metadata,
        apk_spec=apk_spec,
        native_spec=native_spec,
        resources_pathmap_path=resources_pathmap_path))

  container = models.Container(name=container_name,
                               metadata=metadata,
                               section_sizes=section_sizes)
  for symbol in raw_symbols:
    symbol.container = container

  file_format.SortSymbols(raw_symbols, check_already_mostly_sorted=True)
  return raw_symbols


def CreateSizeInfo(build_config,
                   raw_symbols_list,
                   normalize_names=True):
  """Performs operations on all symbols and creates a SizeInfo object."""
  all_raw_symbols = []
  for raw_symbols in raw_symbols_list:
    file_format.CalculatePadding(raw_symbols)

    # Do not call _NormalizeNames() during archive since that method tends to
    # need tweaks over time. Calling it only when loading .size files allows for
    # more flexibility.
    if normalize_names:
      _NormalizeNames(raw_symbols)

    all_raw_symbols += raw_symbols

  # Containers should always have at least one symbol.
  container_list = [syms[0].container for syms in raw_symbols_list]
  return models.SizeInfo(build_config, container_list, all_raw_symbols)


def _DetectGitRevision(directory):
  """Runs git rev-parse to get the SHA1 hash of the current revision.

  Args:
    directory: Path to directory where rev-parse command will be run.

  Returns:
    A string with the SHA1 hash, or None if an error occured.
  """
  try:
    git_rev = subprocess.check_output(
        ['git', '-C', directory, 'rev-parse', 'HEAD']).decode('ascii')
    return git_rev.rstrip()
  except Exception:
    logging.warning('Failed to detect git revision for file metadata.')
    return None


def _ParseGnArgs(args_path):
  """Returns a list of normalized "key=value" strings."""
  args = {}
  with open(args_path) as f:
    for l in f:
      # Strips #s even if within string literal. Not a problem in practice.
      parts = l.split('#')[0].split('=')
      if len(parts) != 2:
        continue
      args[parts[0].strip()] = parts[1].strip()
  return ["%s=%s" % x for x in sorted(args.items())]


def _AddContainerArguments(parser, is_top_args=False):
  """Add arguments applicable to a single container."""

  # Main file argument: Exactly one should be specified (perhaps via -f).
  # _IdentifyInputFile() should be kept updated.
  group = parser.add_argument_group(title='Main Input')
  group = group.add_mutually_exclusive_group(required=True)
  group.add_argument('-f',
                     metavar='FILE',
                     help='Auto-identify input file type.')
  group.add_argument('--apk-file',
                     help='.apk file to measure. Other flags can generally be '
                     'derived when this is used.')
  group.add_argument('--minimal-apks-file',
                     help='.minimal.apks file to measure. Other flags can '
                     'generally be derived when this is used.')
  group.add_argument('--elf-file', help='Path to input ELF file.')
  group.add_argument('--map-file',
                     help='Path to input .map(.gz) file. Defaults to '
                     '{{elf_file}}.map(.gz)?. If given without '
                     '--elf-file, no size metadata will be recorded.')
  group.add_argument('--pak-file',
                     action='append',
                     default=[],
                     dest='pak_files',
                     help='Paths to pak files.')
  if is_top_args:
    group.add_argument('--ssargs-file',
                       help='Path to SuperSize multi-container arguments file.')

  group = parser.add_argument_group(title='What to Analyze')
  group.add_argument('--java-only',
                     action='store_true',
                     help='Run on only Java symbols')
  group.add_argument('--native-only',
                     action='store_true',
                     help='Run on only native symbols')
  group.add_argument('--no-java',
                     action='store_true',
                     help='Do not run on Java symbols')
  group.add_argument('--no-native',
                     action='store_true',
                     help='Do not run on native symbols')
  if is_top_args:
    group.add_argument('--container-filter',
                       help='Regular expression for which containers to create')

  group = parser.add_argument_group(title='Analysis Options for Native Code')
  group.add_argument('--no-string-literals',
                     dest='track_string_literals',
                     default=True,
                     action='store_false',
                     help='Disable breaking down "** merge strings" into more '
                     'granular symbols.')
  group.add_argument('--no-map-file',
                     dest='ignore_linker_map',
                     action='store_true',
                     help='Use debug information to capture symbol sizes '
                     'instead of linker map file.')
  # Used by tests to override path to APK-discovered files.
  group.add_argument('--aux-elf-file', help=argparse.SUPPRESS)
  group.add_argument(
      '--aux-map-file',
      help='Path to linker map to use when --elf-file is provided')

  group = parser.add_argument_group(title='APK options')
  group.add_argument('--mapping-file',
                     help='Proguard .mapping file for deobfuscation.')
  group.add_argument('--resources-pathmap-file',
                     help='.pathmap.txt file that contains a maping from '
                     'original resource paths to shortened resource paths.')
  group.add_argument('--abi-filter',
                     dest='abi_filters',
                     action='append',
                     help='For apks with multiple ABIs, break down native '
                     'libraries for this ABI. Defaults to 64-bit when both '
                     '32 and 64 bit are present.')

  group = parser.add_argument_group(title='Analysis Options for Pak Files')
  group.add_argument('--pak-info-file',
                     help='This file should contain all ids found in the pak '
                     'files that have been passed in. If not specified, '
                     '${pak_file}.info is assumed.')

  group = parser.add_argument_group(title='Analysis Options (shared)')
  group.add_argument('--source-directory',
                     help='Custom path to the root source directory.')
  group.add_argument('--output-directory',
                     help='Path to the root build directory.')
  if is_top_args:
    group.add_argument('--json-config', help='Path to a supersize.json.')
    group.add_argument('--no-output-directory',
                       action='store_true',
                       help='Do not auto-detect --output-directory.')
    group.add_argument('--include-padding',
                       action='store_true',
                       help='Include a padding field for each symbol, '
                       'instead of rederiving from consecutive symbols '
                       'on file load.')
    group.add_argument('--check-data-quality',
                       action='store_true',
                       help='Perform sanity checks to ensure there is no '
                       'missing data.')


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  _AddContainerArguments(parser, is_top_args=True)


def _IdentifyInputFile(args, on_config_error):
  """Identifies main input file type from |args.f|, and updates |args|.

  Identification is performed on filename alone, i.e., the file need not exist.
  The result is written to a field in |args|. If the field exists then it
  simply gets overwritten.

  If '.' is missing from |args.f| then --elf-file is assumed.

  Returns:
    The primary input file.
"""
  if args.f:
    if args.f.endswith('.minimal.apks'):
      args.minimal_apks_file = args.f
    elif args.f.endswith('.apk'):
      args.apk_file = args.f
    elif args.f.endswith('.so') or '.' not in os.path.basename(args.f):
      args.elf_file = args.f
    elif args.f.endswith('.map') or args.f.endswith('.map.gz'):
      args.map_file = args.f
    elif args.f.endswith('.pak'):
      args.pak_files.append(args.f)
    elif args.f.endswith('.ssargs'):
      # Fails if trying to nest them, which should never happen.
      args.ssargs_file = args.f
    else:
      on_config_error('Cannot identify file ' + args.f)
    args.f = None

  ret = [
      args.apk_file, args.elf_file, args.minimal_apks_file,
      args.__dict__.get('ssargs_file'), args.map_file
  ] + (args.pak_files or [])
  ret = [v for v in ret if v]
  if not ret:
    on_config_error(
        'Must pass at least one of --apk-file, --minimal-apks-file, '
        '--elf-file, --map-file, --pak-file, --ssargs-file')
  return ret[0]


def ParseSsargs(lines):
  """Parses .ssargs data.

  An .ssargs file is a text file to specify multiple containers as input to
  SuperSize-archive. After '#'-based comments, start / end whitespaces, and
  empty lines are stripped, each line specifies a distinct container. Format:
  * Positional argument: |name| for the container.
  * Main input file specified by -f, --apk-file, --elf-file, etc.:
    * Can be an absolute path.
    * Can be a relative path. In this case, it's up to the caller to supply the
      base directory.
    * -f switch must not specify another .ssargs file.
  * For supported switches: See _AddContainerArguments().

  Args:
    lines: An iterator containing lines of .ssargs data.
  Returns:
    A list of arguments, one for each container.
  Raises:
    ValueError: Parse error, including input line number.
  """
  sub_args_list = []
  parser = argparse.ArgumentParser(add_help=False)
  parser.error = lambda msg: (_ for _ in ()).throw(ValueError(msg))
  parser.add_argument('name')
  _AddContainerArguments(parser)
  try:
    for lineno, line in enumerate(lines, 1):
      toks = shlex.split(line, comments=True)
      if not toks:  # Skip if line is empty after stripping comments.
        continue
      sub_args_list.append(parser.parse_args(toks))
  except ValueError as e:
    e.args = ('Line %d: %s' % (lineno, e.args[0]), )
    raise e
  return sub_args_list


def _MakeNativeSpec(json_config, **kwargs):
  native_spec = NativeSpec(**kwargs)
  if native_spec.elf_path or native_spec.map_path:
    basename = os.path.basename(native_spec.elf_path or native_spec.map_path)
    native_spec.component = json_config.ComponentForNativeFile(basename)
    native_spec.gen_dir_regex = json_config.GenDirRegexForNativeFile(basename)
    native_spec.source_path_prefix = json_config.SourcePathPrefixForNativeFile(
        basename)

  if not native_spec.map_path:
    # TODO(crbug.com/1193507): Implement string literal tracking without map
    #     files. nm emits some string literal symbols, but most are missing.
    native_spec.track_string_literals = False
    return native_spec

  return native_spec


def _ElfIsMainPartition(elf_path):
  section_ranges = readelf.SectionInfoFromElf(elf_path)
  return models.SECTION_PART_END in section_ranges.keys()


def _DeduceMapPath(elf_path):
  if _ElfIsMainPartition(elf_path):
    map_path = elf_path.replace('.so', '__combined.so') + '.map'
  else:
    map_path = elf_path + '.map'
  if not os.path.exists(map_path):
    map_path += '.gz'
    if not os.path.exists(map_path):
      map_path = None

  if map_path:
    logging.debug('Detected map_path=%s', map_path)
  return map_path


def _CreateNativeSpecs(*, tentative_output_dir, apk_infolist, elf_path,
                       map_path, abi_filters, auto_abi_filters,
                       track_string_literals, ignore_linker_map, json_config,
                       on_config_error):
  if ignore_linker_map:
    map_path = None
  elif (map_path and not map_path.endswith('.map')
        and not map_path.endswith('.map.gz')):
    on_config_error('Expected --map-file to end with .map or .map.gz')
  elif elf_path and not map_path:
    map_path = _DeduceMapPath(elf_path)

  ret = []
  # if --elf-path or --map-path (rather than --aux-elf-path, --aux-map-path):
  if not apk_infolist:
    if map_path or elf_path:
      ret.append(
          _MakeNativeSpec(json_config,
                          apk_so_path=None,
                          map_path=map_path,
                          elf_path=elf_path,
                          track_string_literals=track_string_literals))
    return abi_filters, ret

  lib_infos = [
      f for f in apk_infolist if f.filename.endswith('.so') and f.file_size > 0
  ]

  # Sort so elf_path/map_path applies largest non-filtered library.
  matches_abi = lambda n: not abi_filters or any(f in n for f in abi_filters)
  lib_infos.sort(key=lambda x: (not matches_abi(x.filename), -x.file_size))

  for lib_info in lib_infos:
    apk_so_path = lib_info.filename
    cur_elf_path = None
    cur_map_path = None
    if not matches_abi(apk_so_path):
      logging.debug('Not breaking down %s: secondary ABI', apk_so_path)
    elif apk_so_path.endswith('_partition.so'):
      # TODO(agrieve): Support symbol breakdowns for partitions (they exist in
      #     the __combined .map file. Debug information (nm output) is shared
      #     with base partition.
      logging.debug('Not breaking down %s: partitioned library', apk_so_path)
    else:
      if elf_path:
        # Consume --aux-elf-file for the largest matching binary.
        cur_elf_path = elf_path
        elf_path = None
      elif tentative_output_dir:
        cur_elf_path = os.path.join(
            tentative_output_dir, 'lib.unstripped',
            posixpath.basename(apk_so_path.replace('crazy.', '')))
        if os.path.exists(cur_elf_path):
          logging.debug('Detected elf_path=%s', cur_elf_path)
        else:
          # TODO(agrieve): Not able to find libcrashpad_handler_trampoline.so.
          logging.debug('Not breaking down %s because file does not exist: %s',
                        apk_so_path, cur_elf_path)
          cur_elf_path = None

      if map_path:
        # Consume --aux-map-file for first non-skipped elf.
        cur_map_path = map_path
        map_path = None
      elif cur_elf_path and not ignore_linker_map:
        cur_map_path = _DeduceMapPath(cur_elf_path)

      if auto_abi_filters:
        abi_filters = [posixpath.basename(posixpath.dirname(apk_so_path))]
        logging.info('Detected --abi-filter %s', abi_filters[0])
        auto_abi_filters = False

    ret.append(
        _MakeNativeSpec(json_config,
                        apk_so_path=apk_so_path,
                        map_path=cur_map_path,
                        elf_path=cur_elf_path,
                        track_string_literals=track_string_literals))

  return abi_filters, ret


def _DeduceAuxPaths(args, apk_prefix):
  mapping_path = args.mapping_file
  resources_pathmap_path = args.resources_pathmap_file
  if apk_prefix:
    if not mapping_path:
      mapping_path = apk_prefix + '.mapping'
      logging.debug('Detected --mapping-file=%s', mapping_path)
    if not resources_pathmap_path:
      possible_pathmap_path = apk_prefix + '.pathmap.txt'
      # This could be pointing to a stale pathmap file if path shortening was
      # previously enabled but is disabled for the current build. However, since
      # current apk/aab will have unshortened paths, looking those paths up in
      # the stale pathmap which is keyed by shortened paths would not find any
      # mapping and thus should not cause any issues.
      if os.path.exists(possible_pathmap_path):
        resources_pathmap_path = possible_pathmap_path
        logging.debug('Detected --resources-pathmap-file=%s',
                      resources_pathmap_path)
  return mapping_path, resources_pathmap_path


def _ReadMultipleArgsFromStream(lines, base_dir, err_prefix, on_config_error):
  try:
    ret = ParseSsargs(lines)
  except ValueError as e:
    on_config_error('%s: %s' % (err_prefix, e.args[0]))
  for sub_args in ret:
    for k, v in sub_args.__dict__.items():
      # Translate file arguments to be relative to |sub_dir|.
      if (k.endswith('_file') or k == 'f') and isinstance(v, str):
        sub_args.__dict__[k] = os.path.join(base_dir, v)
  return ret


def _ReadMultipleArgsFromFile(ssargs_file, on_config_error):
  with open(ssargs_file, 'r') as fh:
    lines = list(fh)
  err_prefix = 'In file ' + ssargs_file
  # Supply |base_dir| as the directory containing the .ssargs file, to ensure
  # consistent behavior wherever SuperSize-archive runs.
  base_dir = os.path.dirname(os.path.abspath(ssargs_file))
  return _ReadMultipleArgsFromStream(lines, base_dir, err_prefix,
                                     on_config_error)


# Both |top_args| and |sub_args| may be modified.
def _ProcessContainerArgs(top_args,
                          sub_args,
                          json_config,
                          container_name,
                          on_config_error,
                          apk_path=None,
                          split_name=None):
  sub_args.source_directory = (sub_args.source_directory
                               or top_args.source_directory)
  sub_args.output_directory = (sub_args.output_directory
                               or top_args.output_directory)
  analyze_native = not (sub_args.java_only or sub_args.no_native
                        or top_args.java_only or top_args.no_native)
  analyze_dex = not (sub_args.native_only or sub_args.no_java
                     or top_args.native_only or top_args.no_java)

  apk_path = apk_path or sub_args.apk_file
  if split_name:
    container_name = '{}/{}.apk'.format(container_name, split_name)
    # Make on-demand a part of the name so that:
    # * It's obvious from the name which DFMs are on-demand.
    # * Diffs that change an on-demand status show as adds/removes.
    if _IsOnDemand(apk_path):
      container_name += '?'

  apk_prefix = sub_args.minimal_apks_file or sub_args.apk_file
  if apk_prefix:
    # Allow either .minimal.apks or just .apks.
    apk_prefix = apk_prefix.replace('.minimal.apks', '.aab')
    apk_prefix = apk_prefix.replace('.apks', '.aab')

  mapping_path, resources_pathmap_path = _DeduceAuxPaths(sub_args, apk_prefix)

  apk_spec = None
  apk_infolist = None
  if apk_prefix:
    with zipfile.ZipFile(apk_path) as z:
      apk_infolist = z.infolist()

    apk_spec = ApkSpec(apk_path=apk_path,
                       minimal_apks_path=sub_args.minimal_apks_file,
                       mapping_path=mapping_path,
                       split_name=split_name)
    if top_args.output_directory:
      apk_spec.size_info_prefix = os.path.join(top_args.output_directory,
                                               'size-info',
                                               os.path.basename(apk_prefix))
    apk_spec.analyze_dex = bool(analyze_dex and apk_spec.size_info_prefix)
    apk_spec.default_component = json_config.DefaultComponentForSplit(
        split_name)
    apk_spec.path_defaults = json_config.ApkPathDefaults()

  pak_spec = None
  apk_pak_paths = None
  if apk_spec:
    with zipfile.ZipFile(apk_spec.apk_path) as z:
      apk_pak_paths = [
          f.filename for f in z.infolist() if f.filename.endswith('.pak')
      ]
  if apk_pak_paths or sub_args.pak_files:
    pak_spec = PakSpec(pak_paths=sub_args.pak_files,
                       pak_info_path=sub_args.pak_info_file,
                       apk_pak_paths=apk_pak_paths)

  if analyze_native:
    # Allow top-level --abi-filter to override values set in .ssargs.
    abi_filters = top_args.abi_filters or sub_args.abi_filters
    aux_elf_file = sub_args.aux_elf_file
    aux_map_file = sub_args.aux_map_file
    if split_name not in (None, 'base'):
      aux_elf_file = None
      aux_map_file = None

    auto_abi_filters = not abi_filters and split_name == 'base'
    abi_filters, native_specs = _CreateNativeSpecs(
        tentative_output_dir=top_args.output_directory,
        apk_infolist=apk_infolist,
        elf_path=sub_args.elf_file or aux_elf_file,
        map_path=sub_args.map_file or aux_map_file,
        abi_filters=abi_filters,
        auto_abi_filters=auto_abi_filters,
        track_string_literals=(top_args.track_string_literals
                               and sub_args.track_string_literals),
        ignore_linker_map=(top_args.ignore_linker_map
                           or sub_args.ignore_linker_map),
        json_config=json_config,
        on_config_error=on_config_error)

    # For app bundles, use a consistent ABI for all splits.
    if auto_abi_filters:
      top_args.abi_filters = abi_filters
  else:
    native_specs = []

  logging.info('Container Params: %r', sub_args.__dict__)
  return (sub_args, apk_spec, pak_spec, native_specs, container_name,
          resources_pathmap_path)


def _IsOnDemand(apk_path):
  # Check if the manifest specifies whether or not to extract native libs.
  output = subprocess.check_output([
      path_util.GetAapt2Path(), 'dump', 'xmltree', '--file',
      'AndroidManifest.xml', apk_path
  ]).decode('ascii')

  def parse_attr(name):
    # http://schemas.android.com/apk/res/android:isFeatureSplit(0x0101055b)=true
    # http://schemas.android.com/apk/distribution:onDemand=true
    m = re.search(name + r'(?:\(.*?\))?=(\w+)', output)
    return m and m.group(1) == 'true'

  is_feature_split = parse_attr('android:isFeatureSplit')
  # Can use <dist:on-demand>, or <module dist:onDemand="true">.
  on_demand = parse_attr(
      'distribution:onDemand') or 'distribution:on-demand' in output
  on_demand = bool(on_demand and is_feature_split)

  return on_demand


def _IterSubArgs(top_args, on_config_error):
  """Generates main paths (may be deduced) for each containers given by input.

  Yields:
    For each container, main paths and other info needed to create size_info.
  """
  json_config_path = top_args.json_config
  if not json_config_path:
    json_config_path = path_util.GetDefaultJsonConfigPath()
    logging.info('Using --json-config=%s', json_config_path)
  json_config = json_config_parser.Parse(json_config_path, on_config_error)

  main_file = _IdentifyInputFile(top_args, on_config_error)
  if top_args.no_output_directory:
    top_args.output_directory = None
  else:
    output_directory_finder = path_util.OutputDirectoryFinder(
        value=top_args.output_directory,
        any_path_within_output_directory=main_file)
    top_args.output_directory = output_directory_finder.Finalized()

  if not top_args.source_directory:
    top_args.source_directory = path_util.GetSrcRootFromOutputDirectory(
        top_args.output_directory)
    assert top_args.source_directory

  if top_args.ssargs_file:
    sub_args_list = _ReadMultipleArgsFromFile(top_args.ssargs_file,
                                              on_config_error)
  else:
    sub_args_list = [top_args]

  # Do a quick first pass to ensure inputs have been built.
  for sub_args in sub_args_list:
    main_file = _IdentifyInputFile(sub_args, on_config_error)
    if not os.path.exists(main_file):
      raise Exception('Input does not exist: ' + main_file)

  # Each element in |sub_args_list| specifies a container.
  for sub_args in sub_args_list:
    main_file = _IdentifyInputFile(sub_args, on_config_error)
    if hasattr(sub_args, 'name'):
      container_name = sub_args.name
    else:
      container_name = os.path.basename(main_file)
    if set(container_name) & set('<>?'):
      parser.error('Container name cannot have characters in "<>?"')


    # If needed, extract .apk file to a temp file and process that instead.
    if sub_args.minimal_apks_file:
      for split_name in _ListSplits(sub_args.minimal_apks_file):
        with zip_util.UnzipToTemp(
            sub_args.minimal_apks_file,
            'splits/{}-master.apk'.format(split_name)) as temp:
          yield _ProcessContainerArgs(top_args,
                                      sub_args,
                                      json_config,
                                      container_name,
                                      on_config_error,
                                      apk_path=temp,
                                      split_name=split_name)
    else:
      yield _ProcessContainerArgs(top_args, sub_args, json_config,
                                  container_name, on_config_error)


def Run(top_args, on_config_error):
  path_util.CheckLlvmToolsAvailable()

  if not top_args.size_file.endswith('.size'):
    on_config_error('size_file must end with .size')
  if top_args.check_data_quality:
    start_time = time.time()

  container_re = None
  if top_args.container_filter:
    try:
      container_re = re.compile(top_args.container_filter)
    except Exception as e:
      on_config_error(f'Bad --container-filter input: {e}')

  build_config = None
  seen_container_names = set()
  raw_symbols_list = []
  pak_id_map = pakfile.PakIdMap()

  # Iterate over each container.
  for (sub_args, apk_spec, pak_spec, native_specs, container_name,
       resources_pathmap_path) in _IterSubArgs(top_args, on_config_error):
    if build_config is None:
      # TODO(agrieve): Move this out of the loop.
      build_config = CreateBuildConfig(sub_args.output_directory,
                                       sub_args.source_directory)
    if not native_specs:
      native_specs = [None]

    # TODO(https://crbug.com/1193507): Break down all libraries.
    for native_spec in native_specs[:1]:
      if container_name in seen_container_names:
        raise ValueError('Duplicate container name: {}'.format(container_name))
      seen_container_names.add(container_name)
      if container_re and not container_re.search(container_name):
        logging.info('Skipping filtered container %s', container_name)
        continue
      logging.info('Starting on container %s', container_name)

      metadata = CreateMetadata(apk_spec=apk_spec,
                                native_spec=native_spec,
                                output_directory=sub_args.output_directory)
      raw_symbols = CreateContainerSymbols(
          container_name=container_name,
          metadata=metadata,
          apk_spec=apk_spec,
          pak_spec=pak_spec,
          native_spec=native_spec,
          source_directory=sub_args.source_directory,
          output_directory=sub_args.output_directory,
          resources_pathmap_path=resources_pathmap_path,
          pak_id_map=pak_id_map)
      assert raw_symbols, f'Container {container_name} had no symbols.'

      raw_symbols_list.append(raw_symbols)

  size_info = CreateSizeInfo(build_config,
                             raw_symbols_list,
                             normalize_names=False)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in data_quality.DescribeSizeInfoCoverage(size_info):
      logging.debug(line)
  logging.info('Recorded info for %d symbols', len(size_info.raw_symbols))
  for container in size_info.containers:
    logging.info('Recording metadata: \n  %s',
                 '\n  '.join(describe.DescribeDict(container.metadata)))

  logging.info('Saving result to %s', top_args.size_file)
  file_format.SaveSizeInfo(size_info,
                           top_args.size_file,
                           include_padding=top_args.include_padding)
  size_in_mb = os.path.getsize(top_args.size_file) / 1024.0 / 1024.0
  logging.info('Done. File size is %.2fMiB.', size_in_mb)

  if top_args.check_data_quality:
    logging.info('Checking data quality')
    data_quality.CheckDataQuality(size_info, top_args.track_string_literals)
    duration = (time.time() - start_time) / 60
    if duration > 10:
      raise data_quality.QualityCheckError(
          'Command should not take longer than 10 minutes.'
          ' Took {:.1f} minutes.'.format(duration))
