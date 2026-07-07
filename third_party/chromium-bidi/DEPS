# Copyright 2026 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Declarative dependency management file for gclient.
# Strictly optimized to checkout ONLY compilation and bundling toolchains.

use_relative_paths = True
git_dependencies = 'SYNC'
gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = []

vars = {
  'build_with_chromium': False,

  'build_url': 'https://chromium.googlesource.com/chromium/src/build.git',
  'build_revision': '8896bef21e0491da1232556db3cdfcf192f93018',

  'buildtools_url': 'https://chromium.googlesource.com/chromium/src/buildtools.git',
  'buildtools_revision': 'b1c2cbc21d9a602aac3273f3494bab7b50eb968b',

  'depot_tools_url': 'https://chromium.googlesource.com/chromium/tools/depot_tools.git',
  'depot_tools_revision': '03d4d2ec684257bb66afd11f877105829d94a3d4',

  # GN CIPD package version.
  'gn_version': 'git_revision:618387c6b5c1276885a2282c9609be06441bd1eb',

  # ninja CIPD package version.
  'ninja_version': 'version:3@1.12.1.chromium.4',
}

allowed_hosts = [
  'chromium.googlesource.com',
  'chrome-infra-packages.appspot.com',
]

deps = {
  'buildtools': {
    'url': Var('buildtools_url') + '@' + Var('buildtools_revision'),
    'condition': 'build_with_chromium == False',
  },

  'node_modules': {
    'dep_type': 'gcs',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': 'chromium-bidi/888cf5993c20a0758494796bc4931b2fa85d7ac32f81095cbc02108a3ecdc453',
        'sha256sum': '888cf5993c20a0758494796bc4931b2fa85d7ac32f81095cbc02108a3ecdc453',
        'size_bytes': 17602252,
        'generation': 1783417539972035,
        'output_file': 'node_modules.tar.gz',
      },
    ],
  },

  # Platform-specific GN packages via CIPD
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and build_with_chromium == False',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac" and build_with_chromium == False',
  },
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win" and build_with_chromium == False',
  },

  'build': {
    'url': Var('build_url') + '@' + Var('build_revision'),
    'condition': 'build_with_chromium == False',
  },
  
  'third_party/depot_tools': {
    'url': Var('depot_tools_url') + '@' + Var('depot_tools_revision'),
    'condition': 'build_with_chromium == False',
  },
  
  'third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'build_with_chromium == False',
  },
}

hooks = [
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'condition': 'build_with_chromium == False',
    'action': [
        'vpython3',
        'third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    # Update LASTCHANGE for build script timestamps
    'name': 'lastchange',
    'condition': 'build_with_chromium == False',
    'pattern': '.',
    'action': ['vpython3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
]

recursedeps = [
  'build',
  'buildtools',
]
