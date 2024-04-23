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

  'download_remoteexec_cfg': False,
  'rbe_instance': Str('projects/rbe-chrome-untrusted/instances/default_instance'),
  'reclient_package': 'infra/rbe/client/',
  'reclient_version': 're_client_version:0.138.0.e854224-gomaip',
  'rewrapper_cfg_project': Str(''),

  'chromium_git': 'https://chromium.googlesource.com',

  'abseil_revision': 'a64dd87cec79c80c88190265cfea0cbd4027677f',
  'build_revision': '10bd27f7de8a48c05205b92ccdaf782ae6f2fe17',
  'buildtools_revision': '571c37f17dd4d1de37cc2efb6b32ff1032e47be2',
  'catapult_revision': 'e939ac77bb9471acc10f49e82cfe65790068c3d1',
  'chromium_googletest_revision': 'c5396f931a1319b98da4915f090d00c4d9900601',
  'chromium_re2_revision': '716d20b6550107ddeba09ed39f2f8185cd6a4aab',
  'chromium_testing_revision': '870c20f1d8079296b7cf86b85b0e9a0b9b252342',
  'clang_format_revision': '3c0acd2d4e73dd911309d9e970ba09d58bf23a62',
  'clang_revision': 'bca0c9f60a365ef2294c42e216420c561093f06f',
  'depot_tools_revision': '6ba67afd6fb7718743af91b847ddf1907f3ee9a6',
  'gn_version': 'git_revision:155c53952ec2dc324b0438ce5b9bd4a286577d25',
  'googletest_revision': '5a37b517ad4ab6738556f0284c256cae1466c5b4',
  'libcxx_revision':       'e3b94d0e5b86883fd77696bf10dc33ba250ba99b',
  'libcxxabi_revision':    '51fe3584464d29067c8f0663f6e6dec84cc86868',
  'ninja_version': 'version:2@1.11.1.chromium.6',
  're2_revision': 'b84e3ff189980a33d4a0c6fa1201aa0b3b8bab4a',
}

deps = {
  'third_party/clang-format/script':
     Var('chromium_git') +
     '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
     Var('clang_format_revision'),

  'build': '{chromium_git}/chromium/src/build.git@{build_revision}',
  'buildtools': '{chromium_git}/chromium/src/buildtools.git@{buildtools_revision}',

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

  'buildtools/reclient': {
     'packages': [
       {
         'package': Var('reclient_package') + '${{platform}}',
         'version': Var('reclient_version'),
       }
     ],
     'dep_type': 'cipd',
   },

  'testing': '{chromium_git}/chromium/src/testing.git@{chromium_testing_revision}',
  'third_party/abseil-cpp': '{chromium_git}/chromium/src/third_party/abseil-cpp@{abseil_revision}',
  'third_party/catapult': '{chromium_git}/catapult.git@{catapult_revision}',
  'third_party/depot_tools': '{chromium_git}/chromium/tools/depot_tools.git@{depot_tools_revision}',
  'third_party/googletest':
      '{chromium_git}/chromium/src/third_party/googletest.git@{chromium_googletest_revision}',
  'third_party/googletest/src':
      '{chromium_git}/external/github.com/google/googletest@{googletest_revision}',

  'third_party/libc++/src':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxx.git@{libcxx_revision}',

  'third_party/libc++abi/src':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxxabi.git@{libcxxabi_revision}',

  'third_party/ninja': {
    'packages': [
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
  },

  'third_party/re2':
      '{chromium_git}/chromium/src/third_party/re2.git@{chromium_re2_revision}',
  'third_party/re2/src':
      '{chromium_git}/external/github.com/google/re2.git@{re2_revision}',

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

  # Configure remote exec cfg files
  {
    # Use luci_auth if on windows and using chrome-untrusted project
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and host_os == "win"',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--use_luci_auth_credshelper',
               '--quiet',
               ],
  },  {
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and not host_os == "win"',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--quiet',
               ],
  },
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'not download_remoteexec_cfg',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--skip_remoteexec_cfg_fetch',
               '--quiet',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'action': ['python3',
               'build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
]

