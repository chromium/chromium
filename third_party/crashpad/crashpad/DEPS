# Copyright 2014 The Crashpad Authors. All rights reserved.
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
      '9e121212d42be62a7cce38072f925f8398d11e49',
  'crashpad/third_party/edo/edo': {
      'url': Var('chromium_git') + '/external/github.com/google/eDistantObject.git@' +
      '6ffbf833173f53fcd06ecf08670a95cc01c01f72',
      'condition': 'checkout_ios',
  },
  'crashpad/third_party/googletest/googletest':
      Var('chromium_git') + '/external/github.com/google/googletest@' +
      'e589a337170554c48bc658cc857cf15080c9eacc',
  'crashpad/third_party/gyp/gyp':
      Var('chromium_git') + '/external/gyp@' +
      '8bee09f4a57807136593ddc906b0b213c21f9014',
  'crashpad/third_party/lss/lss':
      Var('chromium_git') + '/linux-syscall-support.git@' +
      '7bde79cc274d06451bf65ae82c012a5d3e476b5a',
  'crashpad/third_party/mini_chromium/mini_chromium':
      Var('chromium_git') + '/chromium/mini_chromium@' +
      '329ca82f73a592d832e79334bed842fba85b9fdd',
  'crashpad/third_party/libfuzzer/src':
      Var('chromium_git') + '/chromium/llvm-project/compiler-rt/lib/fuzzer.git@' +
      'fda403cf93ecb8792cb1d061564d89a6553ca020',
  'crashpad/third_party/zlib/zlib':
      Var('chromium_git') + '/chromium/src/third_party/zlib@' +
      '13dc246a58e4b72104d35f9b1809af95221ebda7',

  # CIPD packages below.
  'crashpad/third_party/linux/clang/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/clang/linux-amd64',
        'version': 'goma',
      },
    ],
    'condition': 'checkout_linux and pull_linux_clang',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/clang/mac-amd64': {
    'packages': [
      {
        'package': 'fuchsia/clang/mac-amd64',
        'version': 'goma',
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "mac"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/clang/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/clang/linux-amd64',
        'version': 'goma',
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "linux"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/sdk/mac-amd64': {
    'packages': [
      {
        'package': 'fuchsia/sdk/gn/mac-amd64',
        'version': 'latest'
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "mac"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/fuchsia/sdk/linux-amd64': {
    'packages': [
      {
        'package': 'fuchsia/sdk/gn/linux-amd64',
        'version': 'latest'
      },
    ],
    'condition': 'checkout_fuchsia and host_os == "linux"',
    'dep_type': 'cipd'
  },
  'crashpad/third_party/win/toolchain': {
    # This package is only updated when the solution in .gclient includes an
    # entry like:
    #   "custom_vars": { "pull_win_toolchain": True }
    # This is because the contained bits are not redistributable.
    'packages': [
      {
        'package': 'chrome_internal/third_party/sdk/windows',
        'version': 'uploaded:2018-06-13'
      },
    ],
    'condition': 'checkout_win and pull_win_toolchain',
    'dep_type': 'cipd'
  },
}

hooks = [
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--no_auth',
      '--bucket=chromium-clang-format',
      '--sha1_file',
      'buildtools/win/clang-format.exe.sha1',
    ],
  },
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
    'name': 'setup_gn_ios',
    'pattern': '.',
    'condition': 'run_setup_ios_gn and checkout_ios',
    'action': [
        'python',
        'crashpad/build/ios/setup_ios_gn.py'
    ],
  },
]

recursedeps = [
  'buildtools',
]
