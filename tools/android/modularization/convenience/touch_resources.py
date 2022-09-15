#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
r"""Creates Android resources directories and boilerplate files for a module.

This is a utility script for conveniently creating resources directories and
values .xml files in modules prefilled with boilerplate and example usages. It
prints out suggested changes to the BUILD.gn and will apply them if accepted.

Examples:

Touch colors.xml and styles.xml in module foo:
   tools/android/modularization/convenience/touch_resources.py \
   chrome/browser/foo \
   -v colors styles

Touch dimens.xml in module foo's internal dir for hdpi, xhdpi and xxdpi:
   tools/android/modularization/convenience/touch_resources.py \
   chrome/browser/foo/internal \
   -v dimens \
   -q hdpi xhdpi xxhdpi

Touch drawable directories in module foo for hdpi, xhdpi and xxdpi:
   tools/android/modularization/convenience/touch_resources.py \
   chrome/browser/foo \
   -d drawable \
   -q hdpi xhdpi xxhdpi
"""

import argparse
import datetime
import pathlib
from typing import List, Optional, Tuple

import build_gn_editor

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

  arg_parser.add_argument('module',
                          help='Module directory to create resources for. e.g. '
                          'chrome/browser/foo')

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
    _touch_subdirectories(resources_path, subdirectory, qualifier_suffixes)

  if not changes_requested:
    print('No resource types specified to create, so just created the res/ '
          'directory. Use -v/--values to create value resources and '
          '-d/--directories to create resources subdirectories.')

  # Print out build target suggestions
  all_resources.sort(key=str)
  if not all_resources:
    return

  build_file = build_gn_editor.BuildFile(build_gn_path)
  build_gn_changes_ok = _update_build_file(build_file, all_resources)

  if not build_gn_changes_ok:
    _print_build_target_suggestions(build_gn_path, all_resources)
    return

  print('Final delta:')
  print(build_file.get_diff())
  apply_changes = _yes_or_no('Would you like to apply these changes?')
  if not apply_changes:
    return

  build_file.write_content_to_file()


def _yes_or_no(question: str) -> bool:
  val = input(question + ' [(y)es/(N)o] ')
  try:
    y_or_n = val.lower().strip()
    return y_or_n[0] == 'y'
  except Exception:
    print('Invalid input. Assuming No.')
    return False


def _determine_target_to_use(targets: List[str], target_type: str,
                             default_name: str) -> Optional[str]:
  num_targets = len(targets)
  if not num_targets:
    print(f'Found no existing {target_type} will create ":{default_name}".')
    return default_name
  if num_targets == 1:
    print(f'Found existing target {target_type}("{targets[0]}"), using it.')
    return targets[0]
  print(f'Found multiple existing {target_type} targets, pick one: ')
  return _enumerate_targets_and_ask(targets)


def _enumerate_targets_and_ask(targets: List[str]) -> Optional[str]:
  for i, target in enumerate(targets):
    print(f'{i + 1}: {target}')

  try:
    val = int(
        input('Enter the number corresponding the to target you want to '
              'use: ')) - 1
  except ValueError:
    return None

  if 0 <= val < len(targets):
    return targets[val]

  return None


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
<!--
Copyright {year} The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
-->

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


def _generate_resources_sources(build_gn_dir_path: pathlib.Path,
                                new_resources: List[pathlib.Path]) -> List[str]:
  return [f'"{str(r.relative_to(build_gn_dir_path))}"' for r in new_resources]


def _list_to_lines(lines: List[str], indent: int) -> str:
  spaces = ' ' * indent
  return '\n'.join([f'{spaces}{line},' for line in lines])


def _generate_suggested_resources_deps() -> List[str]:
  return [f'# "{dep}"' for dep in _COMMON_RESOURCE_DEPS]


def _generate_resources_content(build_gn_path: pathlib.Path,
                                new_resources: List[pathlib.Path], *,
                                include_comment: bool) -> str:
  build_gn_dir_path = build_gn_path.parent
  new_resources_lines = _list_to_lines(
      _generate_resources_sources(build_gn_dir_path, new_resources), 4)
  suggested_deps_lines = _list_to_lines(_generate_suggested_resources_deps(), 4)
  comment = ''
  if include_comment:
    comment = ('\n    # Commonly required resources deps for convenience, ' +
               'add other required deps and remove unnecessary ones.')
  resources_content = f"""sources = [
{new_resources_lines}
  ]

  deps = [{comment}
{suggested_deps_lines}
  ]"""
  return resources_content


def _generate_suggested_resources(build_gn_path: pathlib.Path,
                                  new_resources: List[pathlib.Path]) -> str:
  resources_content = _generate_resources_content(build_gn_path,
                                                  new_resources,
                                                  include_comment=True)
  resources_target_suggestion = f"""
android_resources("java_resources") {{
  {resources_content}
}}"""
  return resources_target_suggestion


def _generate_suggested_java_package(build_gn_path: pathlib.Path) -> str:
  build_gn_dir_path = build_gn_path.parent
  parts_for_package = build_gn_dir_path.parts
  # internal, public or android subdirectories are not part of the Java package.
  while parts_for_package[-1] in ('internal', 'public', 'android'):
    parts_for_package = parts_for_package[:-1]
  return f'org.chromium.{".".join(parts_for_package)}'


def _generate_library_content(build_gn_path: pathlib.Path,
                              resources_target_name: str) -> str:
  suggested_java_package = _generate_suggested_java_package(build_gn_path)
  library_content = f"""deps = [
     ":{resources_target_name}",
  ]

  resources_package = "{suggested_java_package}" """
  return library_content


def _generate_library_target(build_gn_path: pathlib.Path,
                             resources_target_name: str) -> str:
  library_content = _generate_library_content(build_gn_path,
                                              resources_target_name)
  android_library_target_suggestion = f"""
android_library("java") {{
  {library_content}
}}"""
  return android_library_target_suggestion


def _create_or_update_variable_list(target: build_gn_editor.BuildTarget,
                                    variable_name: str,
                                    elements: List[str]) -> None:
  variable = target.get_variable(variable_name)
  if variable:
    variable_list = variable.get_content_as_list()
    if not variable_list:
      raise build_gn_editor.BuildFileUpdateError(
          f'{target.get_type()}("{target.get_name()}") '
          f'{variable_name} is not a list.')

    variable_list.add_elements(elements)
    variable.set_content_from_list(variable_list)
    target.replace_variable(variable)
    return

  variable = build_gn_editor.TargetVariable(variable_name, '')
  variable_list = build_gn_editor.VariableContentList()
  variable_list.add_elements(elements)
  variable.set_content_from_list(variable_list)
  target.add_variable(variable)


def _update_build_file(build_file: build_gn_editor.BuildFile,
                       all_resources: List[pathlib.Path]) -> bool:
  libraries = build_file.get_target_names_of_type('android_library')
  resources = build_file.get_target_names_of_type('android_resources')

  library_target = _determine_target_to_use(libraries, 'android_library',
                                            'java')
  resources_target = _determine_target_to_use(resources, 'android_resources',
                                              'java_resources')
  if not library_target or not resources_target:
    print('Invalid build target selections. Aborting BUILD.gn changes.')
    return False

  try:
    _update_build_targets(build_file, all_resources, library_target,
                          resources_target)
  except build_gn_editor.BuildFileUpdateError as e:
    print(f'Changes to build targets failed: {e}. Aborting BUILD.gn changes.')
    return False

  try:
    build_file.format_content()
  except build_gn_editor.BuildFileUpdateError as e:
    print(f'Formatting BUILD gn failed: {e}\n Aborting BUILD.gn changes')
    return False

  return True


def _update_build_targets(build_file: build_gn_editor.BuildFile,
                          all_resources: List[pathlib.Path],
                          library_target: str, resources_target: str) -> None:
  resources = build_file.get_target('android_resources', resources_target)
  if not resources:
    resources = build_gn_editor.BuildTarget(
        'android_resources', resources_target,
        _generate_resources_content(build_file.get_path(),
                                    all_resources,
                                    include_comment=False))
    build_file.add_target(resources)
  else:
    _create_or_update_variable_list(
        resources, 'sources',
        _generate_resources_sources(build_file.get_path().parent,
                                    all_resources))
    _create_or_update_variable_list(resources, 'deps',
                                    _generate_suggested_resources_deps())
    build_file.replace_target(resources)

  library = build_file.get_target('android_library', library_target)
  if not library:
    library = build_gn_editor.BuildTarget(
        'android_library', library_target,
        _generate_library_content(build_file.get_path(), resources_target))
    build_file.add_target(library)
  else:
    _create_or_update_variable_list(library, 'deps', [f'":{resources_target}"'])

    resources_package = library.get_variable('resources_package')
    if not resources_package:
      resources_package_str = _generate_suggested_java_package(
          build_file.get_path())
      library.add_variable(
          build_gn_editor.TargetVariable('resources_package',
                                         f'"{resources_package_str}"'))
    build_file.replace_target(library)


def _print_build_target_suggestions(build_gn_path: pathlib.Path,
                                    new_resources: List[pathlib.Path]) -> None:
  resources_target_suggestion = _generate_suggested_resources(
      build_gn_path, new_resources)
  android_library_target_suggestion = _generate_library_target(
      build_gn_path, 'java_resources')
  print(f'Suggestion for {build_gn_path}:')
  print(resources_target_suggestion)
  print(android_library_target_suggestion)
  print()


if __name__ == '__main__':
  main()
