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

  'abseil_revision': 'c54e50d9f14471146af9f1f357b3c525c2ff5bb7',
  'build_revision': '0347b49096ca873f595c8e285c9bb3e256131f42',
  'buildtools_revision': '5df641722f2e50623646d702e0046fb68c0f5ce1',
  'catapult_revision': '6fd8bbe723677254ff1130a1a416988b8b3e285e',
  'chromium_googletest_revision': '91e8a4cf5607164178e0455bbea40845c9b0d93c',
  'chromium_re2_revision': '581c1f779dffb7d5ffd4296b79736c9ce961caae',
  'chromium_testing_revision': '6bf7e8e674b4552e12d140378e4394e7970e3fbc',
  'clang_format_revision': 'c2725e0622e1a86d55f14514f2177a39efea4a0e',
  'clang_revision': 'f0aeeca2d4eea4ade7308a31bc4023fc52614332',
  'compiler_rt_revision': 'd4d5a24a5514088089ccbaebda6576da640f04e8',
  'depot_tools_revision': '8e49ae5de847360974fb5352cd23ca2794d57222',
  'gn_version': 'git_revision:c5a0003bcc2ac3f8d128aaffd700def6068e9a76',
  'googletest_revision': '4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  'libcxx_revision':       '454a561737fb62e75ba82bfae9fb07ee64acdd68',
  'libcxxabi_revision':    'de02e5d57052b3b6d5fcd76dccde9380bca39360',
  'llvm_libc_revision':    '74b25173cba70124bff5da97cc339d90c516c5f6',
  'ninja_version': 'version:3@1.12.1.chromium.4',
  're2_revision': 'e7aec5985072c1dbe735add802653ef4b36c231a',
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
  'third_party/compiler-rt/src':
    '{chromium_git}/external/github.com/llvm/llvm-project/compiler-rt.git@{compiler_rt_revision}',
  'third_party/depot_tools': '{chromium_git}/chromium/tools/depot_tools.git@{depot_tools_revision}',
  'third_party/googletest':
      '{chromium_git}/chromium/src/third_party/googletest.git@{chromium_googletest_revision}',
  'third_party/googletest/src':
      '{chromium_git}/external/github.com/google/googletest@{googletest_revision}',

  'third_party/libc++/src':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxx.git@{libcxx_revision}',

  'third_party/libc++abi/src':
      '{chromium_git}/external/github.com/llvm/llvm-project/libcxxabi.git@{libcxxabi_revision}',

  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '1ef7b1d60fb433100c27b4552b44577ab86ef5394531d1fbebc237db64a893fd',
        'size_bytes': 56552908,
        'generation': 1762971374100697,
        'condition': 'host_os == "linux" or checkout_android',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'ec1d88867045b8348659f7a8f677d12aa91d7d61a68603a82bad1926bf57c3b0',
        'size_bytes': 5723188,
        'generation': 1762971374436694,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "linux"',
      },
      {
        'object_name': 'Mac/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'f266b79576d4fc0075e9380b68b8879ec2bc9617c973e7bdea694ec006f43636',
        'size_bytes': 54056416,
        'generation': 1762971376161293,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '6f2d61383a3c0ab28286e5a57b7e755eb14726bb9a73a7737b685488eae18b90',
        'size_bytes': 1010052,
        'generation': 1762971385382392,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '9a282bf252e0c7ac88152844f347428e02970aa22941fb583439ce72134f0161',
        'size_bytes': 5607404,
        'generation': 1762971376526568,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'a7b7caf53f4e722234e85aecfdbb3eeb94608c37394672bebd074d6b2f300362',
        'size_bytes': 45184380,
        'generation': 1762971386895625,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'c5ee70e78ae5aa7a0d9b613ea5a8e21629438f12acb50bca0f7e18fae6abfe0a',
        'size_bytes': 5353832,
        'generation': 1762971387217357,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '483b9b2809c3f53b9640e77d83ca6ab3017a0974979d242198abf23d99639e62',
        'size_bytes': 48337640,
        'generation': 1762971401378315,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'd8b3310760c3a8f5dac4801583f7872601f4ba312742b0bf530f043ce6b6f36f',
        'size_bytes': 2520664,
        'generation': 1762971410370409,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '00c4dab7747534548e2111b3adbdbf9ef561887e18c7d6de4c7e273af799c190',
        'size_bytes': 5742908,
        'generation': 1762971401692156,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  'third_party/llvm-libc/src':
    '{chromium_git}/external/github.com/llvm/llvm-project/libc.git@{llvm_libc_revision}',

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

