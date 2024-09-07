# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Overview: ts_library() supports only a small subset of all possible tsconfig
# configurations. Some options cannot be used in general, and some options
# should only be set through the ts_library() gn args corresponding to them. In
# the latter case, there are requirements for how some of these args can be
# configured. This file contains validation logic for tsconfig files and the gn
# args corresponding to config options to limit the possibility of unsupported
# configurations proliferating in the codebase.

import os
import pathlib

from path_utils import isInAshFolder, getTargetPath

_CWD = os.getcwd().replace('\\', '/')
_HERE_DIR = os.path.dirname(__file__)
_SRC_DIR = os.path.normpath(os.path.join(_HERE_DIR, '..',
                                         '..')).replace('\\', '/')

# Options configured by the ts_library should not be set separately.
_tsconfig_compiler_options_mappings = {
    'composite': 'composite=true',
    'declaration': 'composite=true',
    'inlineSourceMap': 'enable_source_maps=true',
    'inlineSources': 'enable_source_maps=true',
    'outDir': 'out_dir',
    'paths': 'path_mappings',
    'rootDir': 'root_dir',
    'tsBuildInfoFile': 'composite=true',
}

# Allowed options within tsconfig_base.json
_allowed_config_options = [
    'extends',
    'compilerOptions',
]

# Allowed compilerOptions
_allowed_compiler_options = [
    'allowUmdGlobalAccess',
    'isolatedModules',
    'lib',
    'noPropertyAccessFromIndexSignature',
    'noUncheckedIndexedAccess',
    'noUnusedLocals',
    'skipLibCheck',
    'strictPropertyInitialization',
    'typeRoots',
    'types',
    'useDefineForClassFields',
]


def validateTsconfigJson(tsconfig, tsconfig_file, is_base_tsconfig):
  # Special exception for material_web_components, which uses ts_library()
  # in an unsupported way.
  if 'third_party/material_web_components/tsconfig_base.json' in tsconfig_file:
    return True, None

  # TODO(b/267329383): Migrate A11y to TypeScript. Accessibility code has
  # different requirements for the migration because of this we need both
  # allowjs and a custom tsconfig.
  if 'accessibility/tsconfig.base.json' in tsconfig_file:
    return True, None

  if not is_base_tsconfig:
    for param in tsconfig.keys():
      if param not in _allowed_config_options:
        return False, f'Invalid |{param}| option detected in ' + \
            f'{tsconfig_file}.Only |extends| and |compilerOptions| may ' + \
            'be specified.'

  if 'compilerOptions' in tsconfig:
    for param in _tsconfig_compiler_options_mappings.keys():
      if param in tsconfig['compilerOptions']:
        tslibrary_flag = _tsconfig_compiler_options_mappings[param]
        return False, f'Invalid |{param}| flag detected in {tsconfig_file}.' + \
            f' Use the dedicated |{tslibrary_flag}| attribute in '+ \
            'ts_library() instead.'

    if 'ui/file_manager' in tsconfig_file:
      # File manager uses ts_library() in an unsupported way. Just return true
      # here for this special case.
      return True, None

    if not is_base_tsconfig:
      for input_param in tsconfig['compilerOptions'].keys():
        if input_param not in _allowed_compiler_options:
          return False, f'Disallowed |{input_param}| flag detected in '+ \
              f'{tsconfig_file}.'

  return True, None


# Note 1: DO NOT add any directories here corresponding to newly added or
# existing TypeScript code. Instead use TypeScript, which is a requirement.
# Note 2: Only add a directory here if you are in the process of migrating
# legacy JS code to TS. Any new entries here should be accompanied by a bug
# tracking the TS migration.
def validateJavaScriptAllowed(source_dir, out_dir, is_ios):
  # Special case for iOS, which sets the root src/ directory as the source
  # directory for the ts_library() call, see
  # ios/web/public/js_messaging/compile_ts.gni.
  # We don't want to generally allow allow_js anywhere in src/ so check the
  # output directory against the standard ios directories instead. This is a
  # really broad check so also use the platform to make sure this is not abused
  # elsewhere; the iOS use case of using allowJs broadly is not supported.
  if (is_ios and '/ios/' in out_dir):
    return True, None

  # Anything in these ChromeOS-specific directories is allowed to use allow_js.
  # TODO (rbpotter): If possible, standardize the build setup in some of these
  # folders such that they can be more accurately specified in the list below.
  ash_directories = [
      'ash/webui/camera_app_ui/',
      'ash/webui/color_internals/',
      'ash/webui/common/resources/',
      'ash/webui/diagnostics_ui/',
      'ash/webui/file_manager/resources/labs/',
      # TODO(b/314827247): Migrate media_app_ui to TypeScript and remove
      # exception.
      'ash/webui/media_app_ui/',
      # TODO(b/313562946): Migrate help_app_ui mojo pipeline to TypeScript and
      # remove.
      'ash/webui/help_app_ui/',
      # TODO(b/315002705): Migrate shimless_rma to TypeScript and remove
      # exception.
      'ash/webui/shimless_rma/',
      # TODO(b/267329383): Migrate A11y to TypeScript.
      'chrome/browser/resources/chromeos/accessibility',
      'ui/file_manager/',
  ]
  for directory in ash_directories:
    if directory in source_dir:
      return True, None

  # Specific exceptions for directories that are still migrating to TS.
  migrating_directories = [
      # TODO(crbug.com/40848285): Migrate bluetooth-internals to TypeScript and
      # remove exception.
      'chrome/browser/resources/bluetooth_internals',
      'chrome/browser/resources/chromeos/accessibility',
      # TODO(crbug.com/41484340): Migrate to TypeScript.
      'chrome/browser/resources/device_log',
      'chrome/test/data/webui',
      # TODO(crbug.com/40848285): Migrate bluetooth-internals to TypeScript and
      # remove exception.
      'chrome/test/data/webui/bluetooth_internals',
      'chrome/test/data/webui/chromeos',
      'chrome/test/data/webui/chromeos/ash_common',
      # TODO(b/245336251): Migrate diagnostics app tests to Typescript and
      # remove exception.
      'chrome/test/data/webui/chromeos/diagnostics',
      'chrome/test/data/webui/chromeos/nearby_share',
      # TODO(b/315002705): Migrate shimless rma app tests to Typescript and
      # remove exception.
      'chrome/test/data/webui/chromeos/shimless_rma',
      'chrome/test/data/webui/cr_components/chromeos',
      'components/policy/resources/webui',
      'ui/webui/resources/js',
      'ui/webui/resources/mojo',

      # TODO(crbug.com/40280699) : Migrate to TypeScript.
      'chrome/test/data/webui/media_internals',
      'content/browser/resources/media',

      # TODO(b/274059668): Migrate OOBE to TypeScript.
      'chrome/browser/resources/chromeos/login',
  ]
  for directory in migrating_directories:
    if (source_dir.endswith(directory)
        or source_dir.endswith(directory + '/preprocessed')):
      return True, None

  return False, 'Invalid JS file detected for input directory ' + \
      f'{source_dir} and output directory {out_dir}, all new ' + \
      'code should be added in TypeScript.'


def isMappingAllowed(is_ash_target, target_path, mapping_path):
  if is_ash_target:
    return True

  return not isInAshFolder(mapping_path) or target_path in exceptions


# TODO (https://www.crbug.com/1412158): Remove all exceptions below and this
# function; these build targets rely on implicitly unmapped dependencies.
def isUnsupportedJsTarget(gen_dir, root_gen_dir):
  target_path = getTargetPath(gen_dir, root_gen_dir)
  exceptions = [
      'ash/webui/color_internals/resources',
      'chrome/browser/resources/chromeos/accessibility/select_to_speak',
  ]
  return target_path in exceptions


# |root_dir| shouldn't refer to any parent directories. Specifically it should
# be either:
#   - within the folder tree starting at the ts_library() target's location
#   - within the folder tree starting at the ts_library() target's corresponding
#     target_gen_dir location.
def validateRootDir(root_dir, gen_dir, root_gen_dir, is_ios):
  root_gen_dir_from_build = os.path.normpath(os.path.join(
      gen_dir, root_gen_dir)).replace('\\', '/')
  target_path = os.path.relpath(gen_dir,
                                root_gen_dir_from_build).replace('\\', '/')

  # Broadly special casing ios/ for now, since compile_ts.gni relies on
  # unsupported behavior of setting the root_dir to src/.
  # TODO (https://www.crbug.com/1412158): Make iOS TypeScript build tools use
  # ts_library in a supported way, or change them to not rely on ts_library.
  if (is_ios and 'ios' in pathlib.Path(target_path).parts):
    return True, None

  # Legacy cases supported for backward-compatibility. Do not add new targets
  # here. The existing exceptions should be removed over time.
  exceptions = [
      # ChromeOS cases
      'ash/webui/color_internals/mojom',
  ]

  if target_path in exceptions:
    return True, None

  target_path_src = os.path.relpath(os.path.join(_SRC_DIR, target_path),
                                    _CWD).replace('\\', '/')
  root_path_from_gen = os.path.relpath(root_dir,
                                       root_gen_dir_from_build).replace(
                                           '\\', '/')
  root_path_from_src = os.path.relpath(os.path.join(_CWD, root_dir),
                                       _SRC_DIR).replace('\\', '/')

  if (root_path_from_gen.startswith(target_path)
      or root_path_from_src.startswith(target_path)):
    return True, None

  return False, f'Error: root_dir ({root_dir}) should be within {gen_dir} ' + \
      f'or {target_path_src}.'


def validateDefinitionDeps(definitions_files, target_path, gen_dir,
                           root_gen_dir, definitions):
  # Root gen dir relative to the current working directory (essentially 'gen')
  gen_dir_from_build = os.path.normpath(os.path.join(gen_dir,
                                                     root_gen_dir)).replace(
                                                         '\\', '/')

  def getPathFromCwd(exception):
    return os.path.relpath(os.path.join(_SRC_DIR, exception),
                           _CWD).replace('\\', '/')

  # TODO(https://crbug.com/326005022): Determine if the following are actually
  # safe for computation of gn input values.
  exceptions_list = [
      'third_party/material_web_components/',
      'third_party/node/node_modules/',
      'third_party/polymer/v3_0/',
      'tools/typescript/tests/',
  ]
  exceptions = [getPathFromCwd(e) for e in exceptions_list]
  definitions_normalized = [d.replace('\\', '/') for d in definitions]

  missing_inputs = []
  for f in definitions_files:
    # File path relative to the current working directory.
    f_from_cwd = os.path.relpath(f, _CWD).replace('\\', '/')
    is_gen_file = f_from_cwd.startswith(gen_dir_from_build)
    f_from_gen = os.path.relpath(f, gen_dir).replace('\\', '/')
    if not is_gen_file and f_from_gen not in definitions_normalized and \
        not any(f_from_cwd.startswith(exception) for exception in exceptions):
      missing_inputs.append(
          os.path.relpath(f_from_cwd, _SRC_DIR).replace('\\', '/'))

  if not missing_inputs:
    return True, None

  errorMessage = 'Undeclared dependencies to definition files encountered ' + \
                 f'while building {target_path}. Please list the following ' + \
                 'file(s) in |definitions|:\n'
  for missing_input in missing_inputs:
    errorMessage += f'//{missing_input}\n'

  return False, errorMessage
