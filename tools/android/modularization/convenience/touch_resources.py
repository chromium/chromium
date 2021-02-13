#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""Creates Android resources directories and boilerplate files for a module.

This is a utility script for conveniently creating resources directories and
values .xml files in modules prefilled with boilerplate and example usages. It
prints out suggested changes to the BUILD.gn to include those resources but does
not apply them automatically.

Examples:

Touch colors.xml and styles.xml in module foo:
   tools/android/modularization/convenience/touch_resources.py \
   -m chrome/browser/foo \
   -v colors styles

Touch dimens.xml in module foo's internal dir for hdpi, xhdpi and xxdpi:
   tools/android/modularization/convenience/touch_resources.py \
   -m chrome/browser/foo/internal \
   -v dimens \
   -q hdpi xhdpi xxhdpi

Touch drawable directories in module foo for hdpi, xhdpi and xxdpi:
   tools/android/modularization/convenience/touch_resources.py \
   -m chrome/browser/foo \
   -d drawable \
   -q hdpi xhdpi xxhdpi
"""

import argparse
import datetime
import pathlib
import subprocess
from typing import List, Optional, Tuple

_IGNORED_FILES_IN_RES = {'DIR_METADATA', 'OWNERS'}

_VALUES_SUPPORTED = [
    'arrays',
    'colors',
    'dimens',
    'ids',
    'strings',
    'styles',
]

_DIRS_SUPPORTED = [
    'animator',
    'anim',
    'color',
    'drawable',
    'font',
    'mipmap',
    'layout',
    'menu',
    'raw',
    'values',
    'xml',
]


def main():
  arg_parser = argparse.ArgumentParser(
      description='Creates Android resources directories and boilerplate files '
      'for a module.')

  required_arg_group = arg_parser.add_argument_group('required arguments')
  required_arg_group.add_argument('-m',
                                  '--module',
                                  required=True,
                                  help='Module directory to create resources '
                                  'for. e.g. chrome/browser/foo')

  arg_parser.add_argument('-v',
                          '--values',
                          nargs='+',
                          default=[],
                          choices=_VALUES_SUPPORTED,
                          help='Creates values .xml resources files that do '
                          'not exist yet.')
  arg_parser.add_argument(
      '-d',
      '--directories',
      nargs='+',
      default=[],
      choices=_DIRS_SUPPORTED,
      help='Creates resources file directories that do not exist yet. '
      'Use --values to create the values directory.')
  arg_parser.add_argument(
      '-q',
      '--qualifiers',
      nargs='+',
      help='If specified, resources will be created under these Android '
      'resources qualifiers. See '
      'https://developer.android.com/guide/topics/resources/providing-resources#AlternativeResources'
  )

  arguments = arg_parser.parse_args()

  # Recognize directory structure and determine the existing BUILD.gn location
  # and where resources are or should be
  build_gn_path, resources_path = _identify_module_structure(arguments.module)

  # Create res/ directory if it does not exist
  if not resources_path.is_dir():
    resources_path.mkdir(parents=True)
    print(f'Created resources directory: {resources_path}')

  # Detect existing resources
  all_resources = [
      p for p in resources_path.rglob('*')
      if p.is_file() and p.name not in _IGNORED_FILES_IN_RES
  ]

  changes_requested = False
  new_resources = []

  # Process -q/--qualifiers
  if not arguments.qualifiers:
    qualifier_suffixes = ['']
  else:
    qualifier_suffixes = [f'-{qualifier}' for qualifier in arguments.qualifiers]

  # Process -v/--values
  for value_type in arguments.values:
    changes_requested = True
    if value_type == 'strings':
      raise ValueError(
          'strings.xml files are replaced by strings.grd files for '
          'localization, and modules do not need to create separate '
          'strings.grd files. Existing strings can be left in and new strings '
          'can be added to '
          'chrome/browser/ui/android/strings/android_chrome_strings.grd')
    else:
      created_resources = _touch_values_files(resources_path, value_type,
                                              qualifier_suffixes)
      new_resources.extend(created_resources)
      all_resources.extend(created_resources)

  # Process -d/--directories
  for subdirectory in arguments.directories:
    changes_requested = True
    if subdirectory == 'values':
      raise ValueError(
          'Use -v/--values to create the values directory and values resources.'
      )
    else:
      _touch_subdirectories(resources_path, subdirectory, qualifier_suffixes)

  if not changes_requested:
    print('No resource types specified to create, so just created the res/ '
          'directory. Use -v/--values to create value resources and '
          '-d/--directories to create resources subdirectories.')

  # Print out build target suggestions
  all_resources.sort(key=str)
  if all_resources:
    _print_build_target_suggestions(build_gn_path, all_resources)


def _identify_module_structure(path_argument: str
                               ) -> Tuple[pathlib.Path, pathlib.Path]:
  module_path = pathlib.Path(path_argument)
  assert module_path.is_dir()

  # If present, prefer module/android/BUILD.gn
  possible_android_path = module_path / 'android'
  if possible_android_path.is_dir():
    possible_build_gn_path = possible_android_path / 'BUILD.gn'
    if possible_build_gn_path.is_file():
      build_gn_path = possible_build_gn_path
      resources_path = possible_android_path / 'java' / 'res'
      return build_gn_path, resources_path

  # The recommended structure is module/BUILD.gn
  possible_build_gn_path = module_path / 'BUILD.gn'
  if possible_build_gn_path.is_file():
    build_gn_path = possible_build_gn_path
    possible_existing_java_path = module_path / 'java'
    # If module/java exists, use module/java/res, but the preferred structure is
    # module/android/java/res
    if possible_existing_java_path.is_dir():
      resources_path = possible_existing_java_path / 'res'
    else:
      resources_path = possible_android_path / 'java' / 'res'
    return build_gn_path, resources_path

  raise Exception(
      f'BUILD.gn found neither in {module_path} nor in {possible_android_path}')


def _touch_values_files(resources_path: pathlib.Path, value_resource_type: str,
                        qualifier_suffixes: List[str]) -> List[pathlib.Path]:
  created_files = []
  for qualifier_suffix in qualifier_suffixes:
    values_path = resources_path / f'values{qualifier_suffix}'
    values_path.mkdir(parents=True, exist_ok=True)

    xml_path = values_path / f'{value_resource_type}.xml'
    if xml_path.is_file():
      print(f'{xml_path} already exists.')
      continue

    with xml_path.open('a') as f:
      f.write(_create_filler(value_resource_type))
    print(f'Created {xml_path}')
    created_files.append(xml_path)
  return created_files


_RESOURCES_BOILERPLATE_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<!-- Copyright {year} The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<resources xmlns:tools="http://schemas.android.com/tools">
{contents}
</resources>
"""

_DIMENS_BOILERPLATE = """    <!-- Foo icon dimensions -->
    <dimen name="foo_icon_height">24dp</dimen>
    <dimen name="foo_icon_width">24dp</dimen>"""

_COLORS_BOILERPLATE = """    <!-- Foo UI colors -->
    <color name="foo_background_color">@color/default_bg_color_light</color>"""

_STYLES_BOILERPLATE = """    <!-- Styling for a Foo menu button. -->
    <style name="FooMenuButton">
        <item name="android:layout_width">48dp</item>
        <item name="android:layout_height">24dp</item>
        <item name="tint">@color/default_icon_color_tint_list</item>
    </style>"""

_IDS_BOILERPLATE = """    <!-- Dialog button ids -->
    <item type="id" name="foo_ok_button" />
    <item type="id" name="foo_cancel_button" />"""

_ARRAYS_BOILERPLATE = """    <!-- Prime numbers -->
    <integer-array name="foo_primes">
        <item>2</item>
        <item>3</item>
        <item>5</item>
        <item>7</item>
    </integer-array>

    <!-- Geometrics shapes -->
    <array name="foo_shapes">
        <item>@drawable/triangle</item>
        <item>@drawable/square</item>
        <item>@drawable/circle</item>
    </array>"""

_BOILERPLATE = {
    'dimens': _DIMENS_BOILERPLATE,
    'colors': _COLORS_BOILERPLATE,
    'styles': _STYLES_BOILERPLATE,
    'ids': _IDS_BOILERPLATE,
    'arrays': _ARRAYS_BOILERPLATE
}


def _create_filler(value_resource_type: str) -> str:
  boilerplate = _BOILERPLATE[value_resource_type]
  return _RESOURCES_BOILERPLATE_TEMPLATE.format(year=_get_current_year(),
                                                contents=boilerplate)


def _get_current_year() -> int:
  return datetime.datetime.now().year


_COMMON_RESOURCE_DEPS = [
    "//chrome/browser/ui/android/strings:ui_strings_grd",
    "//components/browser_ui/strings/android:browser_ui_strings_grd",
    "//components/browser_ui/styles/android:java_resources",
    "//components/browser_ui/widget/android:java_resources",
    "//third_party/android_deps:material_design_java",
    "//ui/android:ui_java_resources",
]


def _touch_subdirectories(resources_path: pathlib.Path, subdirectory: str,
                          qualifier_suffixes: List[str]) -> List[pathlib.Path]:
  for qualifier_suffix in qualifier_suffixes:
    subdir_name = f'{subdirectory}{qualifier_suffix}'
    subdir_path = resources_path / subdir_name
    if not subdir_path.is_dir():
      subdir_path.mkdir(parents=True)
      print(f'Created {subdir_path}')
    else:
      print(f'{subdir_path} already exists.')


def _print_build_target_suggestions(build_gn_path: pathlib.Path,
                                    new_resources: List[pathlib.Path]) -> None:
  build_gn_dir_path = build_gn_path.parent
  new_resources_strs = [
      str(r.relative_to(build_gn_dir_path)) for r in new_resources
  ]
  new_resources_lines = '\n'.join(
      [f'    "{src}",' for src in new_resources_strs])
  suggested_deps_lines = '\n'.join(
      [f'    # "{dep}",' for dep in _COMMON_RESOURCE_DEPS])
  resources_target_suggestion = f"""
android_resources("java_resources") {{
  sources = [
{new_resources_lines}
  ]
  deps = [
    # Commonly required resources deps for convenience, add other required deps and remove unnecessary ones.
{suggested_deps_lines}
  ]
}}"""
  print(f'Suggestion for {build_gn_path}:')
  print(resources_target_suggestion)

  parts_for_package = build_gn_dir_path.parts
  # internal, public or android subdirectories are not part of the Java package.
  while parts_for_package[-1] in ('internal', 'public', 'android'):
    parts_for_package = parts_for_package[:-1]
  suggested_java_package = f'org.chromium.{".".join(parts_for_package)}'
  android_library_target_suggestion = f"""
android_library("java") {{
  [...]
  deps = [
     ":java_resources",
     [...]
  ]
  resources_package = "{suggested_java_package}"
}}"""
  print(android_library_target_suggestion)
  print()


if __name__ == '__main__':
  main()
