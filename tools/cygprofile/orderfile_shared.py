# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utilities for orderfile generation."""

from typing import Optional
import android_profile_tool
import cluster
import process_profiles


def ReadNonEmptyStrippedFromFile(file_name):
  """Reads a file, strips leading/trailing whitespace and removes empty lines.
  """
  stripped_lines = []
  with open(file_name, 'r') as file:
    for line in file:
      stripped_line = line.strip()
      if stripped_line:
        stripped_lines.append(stripped_line)
  return stripped_lines


def AddDummyFunctions(unpatched_orderfile_path, orderfile_path):
  """Adds dummy functions to the orderfile."""
  symbols = ReadNonEmptyStrippedFromFile(unpatched_orderfile_path)
  with open(orderfile_path, 'w') as f:
    # Make sure the anchor functions are located in the right place, here and
    # after everything else.
    # See the comment in //base/android/library_loader/anchor_functions.cc.
    f.write('dummy_function_start_of_ordered_text\n')
    f.writelines(s + '\n' for s in symbols)
    f.write('dummy_function_end_of_ordered_text\n')


def ProcessProfiles(profile_files, lib_chrome_so_path):
  """Processes profiles to get an ordered list of symbols."""
  profiles = process_profiles.ProfileManager(profile_files)
  processor = process_profiles.SymbolOffsetProcessor(lib_chrome_so_path)
  ordered_symbols = cluster.ClusterOffsets(profiles, processor)
  if not ordered_symbols:
    raise Exception('Failed to get ordered symbols')
  for sym in ordered_symbols:
    assert not sym.startswith('OUTLINED_FUNCTION_'), (
        'Outlined function found in instrumented function, very likely '
        'something has gone very wrong!')
  symbols_size = processor.SymbolsSize(ordered_symbols)
  return ordered_symbols, symbols_size


def CollectProfiles(profiler: android_profile_tool.AndroidProfileTool,
                    profile_webview: bool,
                    arch: str,
                    apk_path_or_browser_name: str,
                    out_dir_str: Optional[str] = None,
                    webview_installer_path: Optional[str] = None):
  """Collects profiles from the device."""
  if profile_webview:
    if not webview_installer_path:
      raise ValueError(
          'webview_installer_path must be provided for webview profiling')
    profiler.InstallAndSetWebViewProvider(webview_installer_path)
    return profiler.CollectWebViewStartupProfile(apk_path_or_browser_name,
                                                 arch, out_dir_str)

  if arch == 'arm64':
    return profiler.CollectSpeedometerProfile(apk_path_or_browser_name,
                                              out_dir_str)
  return profiler.CollectSystemHealthProfile(apk_path_or_browser_name,
                                             out_dir_str)


def GetLibchromeSoPath(out_dir, arch, profile_webview=False):
  """Returns the path to the unstripped libmonochrome.so."""
  libchrome_target = GetLibchromeTarget(arch, profile_webview)
  return str(out_dir / f'lib.unstripped/{libchrome_target}.so')


def GetLibchromeTarget(arch, profile_webview=False):
  """Returns the libmonochrome target name."""
  if profile_webview:
    return 'libwebviewchromium'
  target = 'libmonochrome'
  if '64' in arch:
    # Trichrome has a _64 suffix for arm64 and x64 builds.
    target += '_64'
  return target


def AddCommonArguments(parser):
  """Adds common arguments to the parser."""
  parser.add_argument('--target-arch',
                      dest='arch',
                      required=True,
                      choices=['arm', 'arm64', 'x86', 'x64'],
                      help='The target architecture for which to build.')
  parser.add_argument('--profile-webview',
                      action='store_true',
                      default=False,
                      help='Use the WebView benchmark profiles to generate the '
                      'orderfile.')
  parser.add_argument('--streamline-for-debugging',
                      action='store_true',
                      help=('Streamline the run for faster debugging.'))
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbosity',
                      action='count',
                      default=0,
                      help='Increase verbosity for debugging.')
  parser.add_argument('--save-profile-data',
                      action='store_true',
                      default=False,
                      help='Avoid deleting the generated profile data.')
