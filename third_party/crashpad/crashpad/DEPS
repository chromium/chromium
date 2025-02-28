# Copyright 2014 The Crashpad Authors
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

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'gn_version': 'git_revision:5e19d2fb166fbd4f6f32147fbb2f497091a54ad8',
  # ninja CIPD package version.
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:2@1.8.2.chromium.3',
  'pull_linux_clang': False,
  'pull_win_toolchain': False,
  # Controls whether crashpad/build/ios/setup-ios-gn.py is run as part of
  # gclient hooks. It is enabled by default for developer's convenience. It can
  # be disabled with custom_vars (done automatically on the bots).
  'run_setup_ios_gn': True,
}

deps = {
  'buildtools':
      Var('chromium_git') + '/chromium/src/buildtools.git@' +
      'efa920ce144e4dc1c1841e73179cd7e23b9f0d5e',
  'buildtools/clang_format/script':
      Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
      'c912837e0d82b5ca4b6e790b573b3956d3744c1c',
  'crashpad/third_party/edo/edo': {
      'url': Var('chromium_git') + '/external/github.com/google/eDistantObject.git@' +
      '727e556705278598fce683522beedbb9946bfda0',
      'condition': 'checkout_ios',
  },
  'crashpad/third_party/googletest/googletest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      'af29db7ec28d6df1c7f0f745186884091e602e07',
  'crashpad/third_party/lss/lss':
      Var('chromium_git') + '/linux-syscall-support.git@' +
      '9719c1e1e676814c456b55f5f070eabad6709d31',
  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      'aa56c39732fe3056cc342e59f1a5563ed6ba5e5e',
  'crashpad/third_party/libfuzzer/src':
      Var('chromium_git') + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git@' +
      'fda403cf93ecb8792cb1d061564d89a6553ca020',
  'crashpad/third_party/zlib/zlib':
      Var('chromium_git') + '/chromium/src/third_party/zlib@' +
      'fef58692c1d7bec94c4ed3d030a45a1832a9615d',

  # CIPD packages.
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
  'crashpad/build/fuchsia': {
     'packages': [
       {
        'package': 'chromium/fuchsia/test-scripts',
        'version': 'latest',
       }
     ],
     'condition': 'checkout_fuchsia',
     'dep_type': 'cipd',
  },
  'crashpad/third_party/linux/clang/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/third_party/clang/linux-amd64',
        'version': 'Tpc85d1ZwSlZ6UKl2d96GRUBGNA5JKholOKe24sRDr0C',
      },
    ],
    'condition': 'checkout_linux and pull_linux_clang',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/clang/mac-amd64': {
    'packages': [
      {
        'package': 'fuchsia/third_party/clang/mac-amd64',
        'version': 'latest',
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "mac"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/clang/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/third_party/clang/linux-amd64',
        'version': 'latest',
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "linux"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/windows/clang/win-amd64': {
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Win/clang-llvmorg-20-init-17108-g29ed6000-2.tar.xz',
        'sha256sum': '1c71efd923a91480480d4f31c2fd5f1369e01e14f15776a9454abbce0bc13548',
        'size_bytes': 46357580,
        'generation': 1737590897363452,
      },
    ],
    'condition': 'checkout_win and host_os == "win"',
    'dep_type': 'gcs',
  },
  'crashpad/third_party/fuchsia-gn-sdk': {
    'packages': [
      {
        'package': 'chromium/fuchsia/gn-sdk',
        'version': 'latest'
      },
    ],
    'condition': 'checkout_fuchsia',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/sdk/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/sdk/core/linux-amd64',
        'version': 'latest'
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "linux"',
    'dep_type': 'cipd'
  },
  # depot_tools/ninja wrapper calls third_party/ninja/{ninja, ninja.exe}.
  # crashpad/third_party/ninja/ninja is another wrapper to call linux ninja
  # or mac ninja.
  # This allows crashpad developers to work for multiple platforms on the same
  # machine.
  'crashpad/third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'host_os == "win"',
    'dep_type': 'cipd',
  },
  'crashpad/third_party/ninja/linux': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'host_os == "linux"',
    'dep_type': 'cipd',
  },
  'crashpad/third_party/ninja/mac-amd64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/mac-amd64',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'dep_type': 'cipd',
  },
  'crashpad/third_party/ninja/mac-arm64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/mac-arm64',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'dep_type': 'cipd',
  },
  'crashpad/third_party/win/toolchain': {
    # This package is only updated when the solution in .gclient includes an
    # entry like:
    #   "custom_vars": { "pull_win_toolchain": True }
    # This is because the contained bits are not redistributable.
    'packages': [
      {
        'package': 'chrome_internal/third_party/sdk/windows',
        'version': 'uploaded:2021-04-28'
      },
    ],
    'condition': 'checkout_win and pull_win_toolchain',
    'dep_type': 'cipd'
  },
}

hooks = [
  {
    # If using a local clang ("pull_linux_clang" above), also pull down a
    # sysroot.
    'name': 'sysroot_linux',
    'pattern': '.',
    'condition': 'checkout_linux and pull_linux_clang',
    'action': [
      'crashpad/build/install_linux_sysroot.py',
    ],
  },
  {
    # Avoid introducing unnecessary PRESUBMIT.py file from build/fuchsia.
    # Never fail and ignore the error if the file does not exist.
    'name': 'Remove the PRESUBMIT.py from build/fuchsia',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'rm',
      '-f',
      'crashpad/build/fuchsia/PRESUBMIT.py',
    ],
  },
  {
    'name': 'Generate Fuchsia Build Definitions',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python3',
      'crashpad/build/fuchsia_envs.py',
      'crashpad/build/fuchsia/gen_build_defs.py'
    ],
  },
  {
    'name': 'setup_gn_ios',
    'pattern': '.',
    'condition': 'run_setup_ios_gn and checkout_ios',
    'action': [
        'python3',
        'crashpad/build/ios/setup_ios_gn.py'
    ],
  },
]

recursedeps = [
  'buildtools',
]
