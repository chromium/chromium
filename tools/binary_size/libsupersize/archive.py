# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Main Python API for analyzing binary size."""

import argparse
import collections
import dataclasses
import functools
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
import dex_deobfuscate
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
  # Path to the .pathmap.txt file for the apk. Used to deobfuscate res/ files.
  resources_pathmap_path: str = None
  # Name of the apk split when .apks is being analyzed.
  split_name: str = None
  # Path such as: out/Release/size-info/BaseName
  size_info_prefix: str = None
  # Whether to break down classes.dex.
  analyze_dex: bool = True
  # Whether to create symbols for each string literal.
  track_string_literals: bool = True
  # Dict of apk_path -> source_path, provided by json config.
  path_defaults: dict = None
  # Component to use for symbols when not specified by DIR_METADATA, provided by
  # json config.
  default_component: str = ''
  # Paths to not create .other symbols for.
  ignore_apk_paths: set = dataclasses.field(default_factory=set)


@dataclasses.dataclass
class ContainerSpec:
  container_name: str
  apk_spec: ApkSpec
  pak_spec: PakSpec
  native_spec: NativeSpec
  source_directory: str
  output_directory: str


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
    elif symbol.IsStringLiteral():  # Handles native and DEX strings.
      symbol.full_name = full_name
      symbol.template_name = full_name
      symbol.name = full_name
    elif symbol.IsDex():
      symbol.full_name, symbol.template_name, symbol.name = (
          function_signature.ParseJava(full_name))
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
  before_size_info, after_size_info, _, _ = (file_format.LoadDeltaSizeInfo(
      path, file_obj=file_obj))
  logging.info('Normalizing symbol names')
  _NormalizeNames(before_size_info.raw_symbols)
  _NormalizeNames(after_size_info.raw_symbols)
  logging.info('Loaded %d + %d symbols', len(before_size_info.raw_symbols),
               len(after_size_info.raw_symbols))
  return before_size_info, after_size_info


def CreateBuildConfig(output_directory, source_directory, url=None, title=None):
  """Creates the dict to use for SizeInfo.build_info."""
  logging.debug('Constructing build_config')
  build_config = {}
  if output_directory:
    gn_args = _ParseGnArgs(os.path.join(output_directory, 'args.gn'))
    build_config[models.BUILD_CONFIG_GN_ARGS] = gn_args
    build_config[models.BUILD_CONFIG_OUT_DIRECTORY] = os.path.relpath(
        output_directory, start=source_directory)
  git_rev = _DetectGitRevision(source_directory)
  if git_rev:
    build_config[models.BUILD_CONFIG_GIT_REVISION] = git_rev
  if url is not None:
    build_config[models.BUILD_CONFIG_URL] = url
  if title is not None:
    build_config[models.BUILD_CONFIG_TITLE] = title

  return build_config


def _CreateMetadata(container_spec, elf_info):
  logging.debug('Constructing metadata')
  metadata = {}
  apk_spec = container_spec.apk_spec
  native_spec = container_spec.native_spec
  output_directory = container_spec.output_directory

  # Ensure all paths are relative to output directory to make them hermetic.
  if output_directory:
    shorten_path = lambda path: os.path.relpath(path, output_directory)
  else:
    # If output directory is unavailable, just store basenames.
    shorten_path = os.path.basename

  if apk_spec:
    apk_metadata = apk.CreateMetadata(apk_spec=apk_spec,
                                      include_file_details=not native_spec,
                                      shorten_path=shorten_path)
    assert not (metadata.keys() & apk_metadata.keys())
    metadata.update(apk_metadata)

  if native_spec:
    native_metadata = native.CreateMetadata(native_spec=native_spec,
                                            elf_info=elf_info,
                                            shorten_path=shorten_path)
    assert not (metadata.keys() & native_metadata.keys())
    metadata.update(native_metadata)

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


def _CreateContainerSymbols(container_spec, apk_file_manager,
                            apk_analyzer_results, pak_id_map,
                            component_overrides, dex_deobfuscator_cache):
  container_name = container_spec.container_name
  apk_spec = container_spec.apk_spec
  pak_spec = container_spec.pak_spec
  native_spec = container_spec.native_spec
  output_directory = container_spec.output_directory
  source_directory = container_spec.source_directory

  logging.info('Starting on container: %s', container_spec)

  raw_symbols = []
  section_sizes = {}
  metrics_by_file = {}
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
                                      component_overrides,
                                      default_component=default_component)
    raw_symbols.extend(new_raw_symbols)

  elf_info = None
  if native_spec:
    section_ranges, native_symbols, elf_info, native_metrics_by_file = (
        native.CreateSymbols(apk_spec=apk_spec,
                             native_spec=native_spec,
                             output_directory=output_directory,
                             pak_id_map=pak_id_map))
    add_syms(section_ranges,
             native_symbols,
             source_path_prefix=native_spec.source_path_prefix,
             component=native_spec.component,
             paths_already_normalized=True)
    metrics_by_file.update(native_metrics_by_file)
  elif apk_spec and apk_spec.analyze_dex:
    logging.info('Analyzing DEX')
    apk_infolist = apk_file_manager.InfoList(apk_spec.apk_path)
    dex_total_size = sum(i.file_size for i in apk_infolist
                         if i.filename.endswith('.dex'))
    if dex_total_size > 0:
      mapping_path = apk_spec.mapping_path  # May be None.
      class_deobfuscation_map = (
          dex_deobfuscator_cache.GetForMappingFile(mapping_path))
      section_ranges, dex_symbols, dex_metrics_by_file = (
          apkanalyzer.CreateDexSymbols(apk_spec.apk_path,
                                       apk_analyzer_results[container_name],
                                       dex_total_size, class_deobfuscation_map,
                                       apk_spec.size_info_prefix,
                                       apk_spec.track_string_literals))
      add_syms(section_ranges, dex_symbols)
      metrics_by_file.update(dex_metrics_by_file)

  if pak_spec:
    section_ranges, pak_symbols = _CreatePakSymbols(
        pak_spec=pak_spec,
        pak_id_map=pak_id_map,
        apk_spec=apk_spec,
        output_directory=output_directory)
    add_syms(section_ranges, pak_symbols)
  apk_metadata = {}

  # This function can get called multiple times for the same APK file, to
  # process .so files that are treated as containers. The |not native_spec|
  # condition below skips these cases to prevent redundant symbol creation.
  if not native_spec and apk_spec:
    logging.info('Analyzing ARSC')
    arsc_section_ranges, arsc_symbols, arsc_metrics_by_file = (
        apk.CreateArscSymbols(apk_spec))
    add_syms(arsc_section_ranges, arsc_symbols)
    metrics_by_file.update(arsc_metrics_by_file)

    other_section_ranges, other_symbols, apk_metadata, apk_metrics_by_file = (
        apk.CreateApkOtherSymbols(apk_spec))
    add_syms(other_section_ranges, other_symbols)
    metrics_by_file.update(apk_metrics_by_file)

  metadata = _CreateMetadata(container_spec, elf_info)
  assert not (metadata.keys() & apk_metadata.keys())
  metadata.update(apk_metadata)
  container = models.Container(name=container_name,
                               metadata=metadata,
                               section_sizes=section_sizes,
                               metrics_by_file=metrics_by_file)
  for symbol in raw_symbols:
    symbol.container = container

  return raw_symbols


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
  group.add_argument('--symbols-dir',
                     default='lib.unstripped',
                     help='Relative path containing unstripped .so files '
                     '(for symbols) w.r.t. the output directory.')
  group.add_argument('--no-string-literals',
                     action='store_true',
                     help=('Do not create symbols for string literals '
                           '(applies to DEX and Native).'))
  if is_top_args:
    group.add_argument('--json-config', help='Path to a supersize.json.')
    group.add_argument('--no-output-directory',
                       action='store_true',
                       help='Do not auto-detect --output-directory.')
    group.add_argument('--check-data-quality',
                       action='store_true',
                       help='Perform sanity checks to ensure there is no '
                       'missing data.')


def AddArguments(parser):
  parser.add_argument('size_file', help='Path to output .size file.')
  parser.add_argument('--title',
                      help='Value for the "title" build_config entry.')
  parser.add_argument('--url', help='Value for the "url" build_config entry.')
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
    # TODO(crbug.com/40757867): Implement string literal tracking without map
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


def _CreateNativeSpecs(*, tentative_output_dir, symbols_dir, apk_infolist,
                       elf_path, map_path, abi_filters, auto_abi_filters,
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
        # TODO(crbug.com/40229168): Remove handling the legacy library prefix
        # 'crazy.' when there is no longer interest in size comparisons for
        # these pre-N APKs.
        cur_elf_path = os.path.join(
            tentative_output_dir, symbols_dir,
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


# Cache to prevent excess log messages.
@functools.lru_cache
def _DeduceMappingPath(mapping_path, apk_prefix):
  if apk_prefix:
    if not mapping_path:
      possible_mapping_path = apk_prefix + '.mapping'
      if os.path.exists(possible_mapping_path):
        mapping_path = possible_mapping_path
        logging.debug('Detected --mapping-file=%s', mapping_path)
      else:
        logging.warning('Could not find proguard mapping file at %s',
                        possible_mapping_path)
  return mapping_path


# Cache to prevent excess log messages.
@functools.lru_cache
def _DeducePathmapPath(resources_pathmap_path, apk_prefix):
  if apk_prefix:
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
      # Path shortening is optional, so do not warn for missing file.
  return resources_pathmap_path


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
def _CreateContainerSpecs(apk_file_manager,
                          top_args,
                          sub_args,
                          json_config,
                          base_container_name,
                          on_config_error,
                          split_name=None):
  sub_args.source_directory = (sub_args.source_directory
                               or top_args.source_directory)
  sub_args.output_directory = (sub_args.output_directory
                               or top_args.output_directory)
  analyze_native = not (sub_args.java_only or sub_args.no_native
                        or top_args.java_only or top_args.no_native)
  analyze_dex = not (sub_args.native_only or sub_args.no_java
                     or top_args.native_only or top_args.no_java)

  if split_name:
    apk_path = apk_file_manager.SplitPath(sub_args.minimal_apks_file,
                                          split_name)
    base_container_name = f'{base_container_name}/{split_name}.apk'
    # Make on-demand a part of the name so that:
    # * It's obvious from the name which DFMs are on-demand.
    # * Diffs that change an on-demand status show as adds/removes.
    if _IsOnDemand(apk_path):
      base_container_name += '?'
  else:
    apk_path = sub_args.apk_file

  apk_prefix = sub_args.minimal_apks_file or sub_args.apk_file
  if apk_prefix:
    # Allow either .minimal.apks or just .apks.
    apk_prefix = apk_prefix.replace('.minimal.apks', '.aab')
    apk_prefix = apk_prefix.replace('.apks', '.aab')

  mapping_path = None
  if analyze_dex:
    mapping_path = _DeduceMappingPath(sub_args.mapping_file, apk_prefix)
  resources_pathmap_path = _DeducePathmapPath(sub_args.resources_pathmap_file,
                                              apk_prefix)
  apk_spec = None
  if apk_prefix:
    apk_spec = ApkSpec(apk_path=apk_path,
                       minimal_apks_path=sub_args.minimal_apks_file,
                       mapping_path=mapping_path,
                       resources_pathmap_path=resources_pathmap_path,
                       split_name=split_name)
    if top_args.output_directory:
      apk_spec.size_info_prefix = os.path.join(top_args.output_directory,
                                               'size-info',
                                               os.path.basename(apk_prefix))
    apk_spec.analyze_dex = analyze_dex
    apk_spec.track_string_literals = not (top_args.no_string_literals
                                          or sub_args.no_string_literals)
    apk_spec.default_component = json_config.DefaultComponentForSplit(
        split_name)
    apk_spec.path_defaults = json_config.ApkPathDefaults()

  pak_spec = None
  apk_pak_paths = None
  apk_infolist = None
  if apk_spec:
    apk_infolist = apk_file_manager.InfoList(apk_path)
    apk_pak_paths = [
        f.filename for f in apk_infolist
        if archive_util.RemoveAssetSuffix(f.filename).endswith('.pak')
    ]
  if not top_args.no_output_directory and (apk_pak_paths or sub_args.pak_files):
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
        symbols_dir=sub_args.symbols_dir,
        apk_infolist=apk_infolist,
        elf_path=sub_args.elf_file or aux_elf_file,
        map_path=sub_args.map_file or aux_map_file,
        abi_filters=abi_filters,
        auto_abi_filters=auto_abi_filters,
        track_string_literals=not (top_args.no_string_literals
                                   or sub_args.no_string_literals),
        ignore_linker_map=(top_args.ignore_linker_map
                           or sub_args.ignore_linker_map),
        json_config=json_config,
        on_config_error=on_config_error)

    # For app bundles, use a consistent ABI for all splits.
    if auto_abi_filters:
      top_args.abi_filters = abi_filters
  else:
    native_specs = []

  ret = [
      ContainerSpec(container_name=base_container_name,
                    apk_spec=apk_spec,
                    pak_spec=pak_spec,
                    native_spec=None,
                    source_directory=sub_args.source_directory,
                    output_directory=sub_args.output_directory)
  ]
  if apk_spec is None:
    # Special case for when pointed at a single ELF, use just one container.
    assert len(native_specs) <= 1
    ret[0].native_spec = native_specs[0] if native_specs else None
  else:
    apk_spec.ignore_apk_paths.update(s.apk_so_path for s in native_specs)
    if pak_spec and pak_spec.apk_pak_paths:
      apk_spec.ignore_apk_paths.update(pak_spec.apk_pak_paths)
    if apk_spec.analyze_dex:
      apk_spec.ignore_apk_paths.update(i.filename for i in apk_infolist
                                       if i.filename.endswith('.dex'))
    apk_spec.ignore_apk_paths.add(apk.RESOURCES_ARSC_FILE)

    for native_spec in native_specs:
      so_name = posixpath.basename(native_spec.apk_so_path)
      abi = posixpath.basename(posixpath.dirname(native_spec.apk_so_path))
      container_name = f'{base_container_name}/{so_name} ({abi})'
      # Use same apk_spec so that all containers for the apk_spec can be found.
      ret.append(
          ContainerSpec(container_name=container_name,
                        apk_spec=apk_spec,
                        pak_spec=None,
                        native_spec=native_spec,
                        source_directory=sub_args.source_directory,
                        output_directory=sub_args.output_directory))
  return ret


def _IsOnDemand(apk_path):
  # Check if the manifest specifies whether or not to extract native libs.
  output = subprocess.check_output([
      path_util.GetAapt2Path(), 'dump', 'xmltree', '--file',
      'AndroidManifest.xml', apk_path
  ]).decode('ascii')

  def parse_attr(namespace, name):
    # A: http://schemas.android.com/apk/res/android:isFeatureSplit(0x...)=true
    # A: http://schemas.android.com/apk/distribution:onDemand=true
    m = re.search(f'A: (?:.*?/{namespace}:)?{name}' + r'(?:\(.*?\))?=(\w+)',
                  output)
    return m and m.group(1) == 'true'

  is_feature_split = parse_attr('android', 'isFeatureSplit')
  # Can use <dist:on-demand>, or <module dist:onDemand="true">.
  on_demand = parse_attr('distribution', 'onDemand') or 'on-demand' in output
  on_demand = bool(on_demand and is_feature_split)

  return on_demand


def _CreateAllContainerSpecs(apk_file_manager, top_args, json_config,
                             on_config_error):
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
  ret = []
  for sub_args in sub_args_list:
    main_file = _IdentifyInputFile(sub_args, on_config_error)
    if hasattr(sub_args, 'name'):
      container_name = sub_args.name
    else:
      container_name = os.path.basename(main_file)
    if set(container_name) & set('<>?'):
      parser.error('Container name cannot have characters in "<>?"')


    if sub_args.minimal_apks_file:
      split_names = apk_file_manager.ExtractSplits(sub_args.minimal_apks_file)
      for split_name in split_names:
        ret += _CreateContainerSpecs(apk_file_manager,
                                     top_args,
                                     sub_args,
                                     json_config,
                                     container_name,
                                     on_config_error,
                                     split_name=split_name)
    else:
      ret += _CreateContainerSpecs(apk_file_manager, top_args, sub_args,
                                   json_config, container_name, on_config_error)
  all_names = [c.container_name for c in ret]
  assert len(set(all_names)) == len(all_names), \
      'Found duplicate container names: ' + '\n'.join(sorted(all_names))

  return ret


def _FilterContainerSpecs(container_specs, container_re=None):
  ret = []
  seen_container_names = set()
  for container_spec in container_specs:
    container_name = container_spec.container_name
    if container_name in seen_container_names:
      raise ValueError('Duplicate container name: {}'.format(container_name))
    seen_container_names.add(container_name)

    if container_re and not container_re.search(container_name):
      logging.info('Skipping filtered container %s', container_name)
      continue
    ret.append(container_spec)
  return ret


def CreateSizeInfo(container_specs, build_config, json_config,
                   apk_file_manager):
  def sort_key(container_spec):
    # Native containers come first to ensure pak_id_map is populated before
    # any pak_spec is encountered.
    if container_spec.native_spec:
      # Do the most complicated container first, since its most likely to fail.
      if container_spec.native_spec.algorithm == 'linker_map':
        native_key = 0
      elif container_spec.native_spec.algorithm == 'dwarf':
        native_key = 1
      else:
        native_key = 2
    else:
      native_key = 3
    return (native_key, container_spec.container_name)

  container_specs.sort(key=sort_key)

  dex_containers = [
      c for c in container_specs
      if not c.native_spec and c.apk_spec and c.apk_spec.analyze_dex
  ]
  # Running ApkAnalyzer concurrently saves ~30 seconds for Monochrome.apks.
  logging.info('Kicking of ApkAnalyzer for %d .apk files', len(dex_containers))
  apk_analyzer_results = {}
  for container_spec in dex_containers:
    apk_analyzer_results[container_spec.container_name] = (
        apkanalyzer.RunApkAnalyzerAsync(container_spec.apk_spec.apk_path,
                                        container_spec.apk_spec.mapping_path))

  raw_symbols_list = []
  pak_id_map = pakfile.PakIdMap()
  dex_deobfuscator_cache = dex_deobfuscate.CachedDexDeobfuscators()
  for container_spec in container_specs:
    raw_symbols = _CreateContainerSymbols(container_spec, apk_file_manager,
                                          apk_analyzer_results, pak_id_map,
                                          json_config.ComponentOverrides(),
                                          dex_deobfuscator_cache)
    assert raw_symbols, f'{container_spec.container_name} had no symbols.'
    raw_symbols_list.append(raw_symbols)

  # Normalize names before sorting.
  logging.info('Normalizing symbol names')
  for raw_symbols in raw_symbols_list:
    _NormalizeNames(raw_symbols)

  # Sorting must happen after normalization.
  logging.info('Sorting symbols')
  for raw_symbols in raw_symbols_list:
    file_format.SortSymbols(raw_symbols)

  logging.debug('Accumulating symbols')
  # Containers should always have at least one symbol.
  container_list = [syms[0].container for syms in raw_symbols_list]
  all_raw_symbols = []
  for raw_symbols in raw_symbols_list:
    all_raw_symbols += raw_symbols

  file_format.CalculatePadding(all_raw_symbols)

  return models.SizeInfo(build_config, container_list, all_raw_symbols)


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

  json_config_path = top_args.json_config
  if not json_config_path:
    json_config_path = path_util.GetDefaultJsonConfigPath()
    logging.info('Using --json-config=%s', json_config_path)
  json_config = json_config_parser.Parse(json_config_path, on_config_error)

  with zip_util.ApkFileManager() as apk_file_manager:
    container_specs = _CreateAllContainerSpecs(apk_file_manager, top_args,
                                               json_config, on_config_error)
    container_specs = _FilterContainerSpecs(container_specs, container_re)

    build_config = CreateBuildConfig(top_args.output_directory,
                                     top_args.source_directory,
                                     url=top_args.url,
                                     title=top_args.title)
    size_info = CreateSizeInfo(container_specs, build_config, json_config,
                               apk_file_manager)

  if logging.getLogger().isEnabledFor(logging.DEBUG):
    for line in data_quality.DescribeSizeInfoCoverage(size_info):
      logging.debug(line)
  logging.info('Recorded info for %d symbols', len(size_info.raw_symbols))
  for container in size_info.containers:
    logging.info('Recording metadata: \n  %s',
                 '\n  '.join(describe.DescribeDict(container.metadata)))

  logging.info('Saving result to %s', top_args.size_file)
  file_format.SaveSizeInfo(size_info, top_args.size_file)
  size_in_mb = os.path.getsize(top_args.size_file) / 1024.0 / 1024.0
  logging.info('Done. File size is %.2fMiB.', size_in_mb)

  if top_args.check_data_quality:
    logging.info('Checking data quality')
    data_quality.CheckDataQuality(size_info, not top_args.no_string_literals)
    duration = (time.time() - start_time) / 60
    if duration > 10:
      raise data_quality.QualityCheckError(
          'Command should not take longer than 10 minutes.'
          ' Took {:.1f} minutes.'.format(duration))
