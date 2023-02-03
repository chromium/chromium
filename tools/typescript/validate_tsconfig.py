# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Options configured by the ts_library should not be set separately.
_tsconfig_compiler_options_mappings = {
    'composite': 'composite=true',
    'declaration': 'composite=true',
    'tsBuildInfoFile': 'composite=true',
    'rootDir': 'root_dir',
    'outDir': 'out_dir',
    'paths': 'path_mappings',
}

# Allowed options within tsconfig_base.json
_allowed_config_options = [
    'extends',
    'compilerOptions',
]

# Allowed compilerOptions
_allowed_compiler_options = [
    'typeRoots',
    'types',
    'noUncheckedIndexedAccess',
    'noUnusedLocals',
    'allowJs',
    'strictPropertyInitialization',
    'noPropertyAccessFromIndexSignature',
    'allowUmdGlobalAccess',
    'sourceMap',
    'inlineSourceMap',
    'inlineSources',
    'skipLibCheck',
]


def ValidateTsconfigJson(tsconfig, tsconfig_file, is_base_tsconfig):
  # Special exception for material_web_components, which uses ts_library()
  # in an unsupported way.
  if 'third_party/material_web_components/tsconfig_base.json' in tsconfig_file:
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

    if 'ui/file_manager/tsconfig_base.json' in tsconfig_file:
      # File manager uses ts_library() in an unsupported way. Just return true
      # here for this special case.
      return True, None

    if not is_base_tsconfig:
      for input_param in tsconfig['compilerOptions'].keys():
        if input_param not in _allowed_compiler_options:
          return False, f'Disallowed |{input_param}| flag detected in '+ \
              f'{tsconfig_file}.'

  return True, None
