use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'build_with_chromium',
  'generate_location_tags',
]

skip_child_includes = [
  'src',
]

vars = {
  'build_with_chromium': False,
  'generate_location_tags': False,

  'chromium_git': 'https://chromium.googlesource.com',

  'abseil_revision': 'c1aaebb624a60dacbc75547ff8771d8cae57293f',
  'build_revision': '6be968106c392e4d179622e443ba586b73a958a6',
  'buildtools_revision': '2ff42d2008f09f65de12e70c6ff0ad58ddb090ad',
  'catapult_revision': 'bdf25f32dca0cdcc633b6d19d886c0aeeba23527',
  'chromium_googletest_revision': '39342c077fc0eaa3ba17d5f51a774b843a20df48',
  'chromium_testing_revision': 'e0a8fc7153fbd4f2d19f58cb60c981c5dcb528d0',
  'clang_format_revision': 'f97059df7f8b205064625cdb5f97b56668a125ef',
  'clang_revision': 'effd9257d456f2d42e9e22fa4f37a24d8cf0b5b5',
  'depot_tools_revision': '62c7a8bad074fdb4339e544b0053206a4f820e15',
  'gn_version': 'git_revision:edf6ef4b06b42c58292faea78498aff76bdf68ed',
  'googletest_revision': 'af29db7ec28d6df1c7f0f745186884091e602e07',
  'libcxx_revision':       '6f4617b9efc36525e030ec9855114f3c93550ec1',
  'libcxxabi_revision':    'f7460fc60ab56553f0b3b0853f1ea60aa51b9478',
  'ninja_version': 'version:2@1.11.1.chromium.6',
}

deps = {
  'build': '{chromium_git}/chromium/src/build.git@{build_revision}',
  'buildtools': '{chromium_git}/chromium/src/buildtools.git@{buildtools_revision}',

  'buildtools/clang_format/script':
      '{chromium_git}/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
      '{clang_format_revision}',

  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      },
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },

  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      },
    ],
  },

  'buildtools/third_party/libc++/trunk':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxx.git@{libcxx_revision}',

  'buildtools/third_party/libc++abi/trunk':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxxabi.git@{libcxxabi_revision}',

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

  'testing': '{chromium_git}/chromium/src/testing.git@{chromium_testing_revision}',
  'third_party/abseil-cpp': '{chromium_git}/chromium/src/third_party/abseil-cpp@{abseil_revision}',
  'third_party/catapult': '{chromium_git}/catapult.git@{catapult_revision}',
  'third_party/depot_tools': '{chromium_git}/chromium/tools/depot_tools.git@{depot_tools_revision}',
  'third_party/googletest':
      '{chromium_git}/chromium/src/third_party/googletest.git@{chromium_googletest_revision}',
  'third_party/googletest/src':
      '{chromium_git}/external/github.com/google/googletest@{googletest_revision}',

  'third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
  },

  'tools/clang': {
    'url': '{chromium_git}/chromium/src/tools/clang.git@{clang_revision}',
    'condition': 'not build_with_chromium',
  },
}

hooks = [
  # Download and initialize "vpython" VirtualEnv environment packages for
  # Python3. We do this before running any other hooks so that any other
  # hooks that might use vpython don't trip over unexpected issues and
  # don't run slower than they might otherwise need to.
  {
    'name': 'vpython3_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', '.vpython3',
                '-vpython-tool', 'install',
    ],
  },

  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x86'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    'action': ['python3', 'build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=x64'],
  },

  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },

  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac or checkout_ios',
    'action': ['python3', 'build/mac_toolchain.py'],
  },

  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac or checkout_ios',
    'action': ['python3', 'build/mac_toolchain.py'],
  },

  {
    # Update the prebuilt clang toolchain.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang',
    'pattern': '.',
    'action': ['python3', 'tools/clang/scripts/update.py'],
  },

  # Update LASTCHANGE.
  {
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'build/util/lastchange.py', '-o',
               'build/util/LASTCHANGE']
  },

  # Don't let the DEPS'd-in depot_tools self-update.
  {
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
      'python3',
      'third_party/depot_tools/update_depot_tools_toggle.py',
      '--disable',
    ],
  },

  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'condition': 'host_os == "mac"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'condition': 'host_os == "linux"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'buildtools/linux64/clang-format.sha1',
    ],
  },

]

