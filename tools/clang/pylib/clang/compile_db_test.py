#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Tests for compile_db."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import compile_db


def _GetGnPath():
  if sys.platform.startswith('linux'):
    subdir, exe = 'linux64', 'gn'
  elif sys.platform == 'darwin':
    subdir, exe = 'mac', 'gn'
  else:
    subdir, exe = 'win', 'gn.exe'
  return os.path.join(
    compile_db.GetChromiumSrcRoot(),
    'buildtools',
    subdir,
    exe,
  )


# Input compile DB.
_TEST_COMPILE_DB = [
  # Includes a string define.
  {
    'command': r'clang-cl.exe /blah "-DCR_CLANG_REVISION=\"346388-1\""',
  },
  # Includes a string define with a space in it.
  {
    'command': r'clang-cl.exe /blah -D"MY_DEFINE=\"MY VALUE\""',
  },
]

# Expected compile DB after processing for windows.
_EXPECTED_COMPILE_DB = [
  {
    'command': r'clang-cl.exe --driver-mode=cl /blah '
    r'"-DCR_CLANG_REVISION=\"346388-1\""',
  },
  {
    'command': r'clang-cl.exe --driver-mode=cl /blah '
    r'-D"MY_DEFINE=\"MY VALUE\""',
  },
]


class CompileDbTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None
    self._orig_platform = sys.platform

  def tearDown(self):
    sys.platform = self._orig_platform

  def testProcessNotOnWindows(self):
    sys.platform = 'linux2'
    processed_compile_db = compile_db.ProcessCompileDatabase(
      _TEST_COMPILE_DB, []
    )

    # Assert no changes were made.
    try:
      # assertItemsEqual is renamed assertCountEqual in Python3.
      self.assertCountEqual(processed_compile_db, _TEST_COMPILE_DB)
    except AttributeError:
      self.assertItemsEqual(processed_compile_db, _TEST_COMPILE_DB)

  def testProcessForWindows_HostPlatformBased(self):
    sys.platform = 'win32'
    processed_compile_db = compile_db.ProcessCompileDatabase(
      _TEST_COMPILE_DB, []
    )

    # Check each entry individually to improve readability of the output.
    for actual, expected in zip(processed_compile_db, _EXPECTED_COMPILE_DB):
      self.assertDictEqual(actual, expected)

  def testProcessForWindows_TargetOsBased(self):
    sys.platform = 'linux2'
    processed_compile_db = compile_db.ProcessCompileDatabase(
      _TEST_COMPILE_DB, [], target_os='win'
    )

    # Check each entry individually to improve readability of the output.
    for actual, expected in zip(processed_compile_db, _EXPECTED_COMPILE_DB):
      self.assertDictEqual(actual, expected)

  def testFrontendArgsFiltered(self):
    sys.platform = 'linux2'
    input_db = [
      {'command': r'clang -g -Xclang -fuse-ctor-homing -funroll-loops test.cc'}
    ]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(input_db, []),
      [{'command': r'clang -g -funroll-loops test.cc'}],
    )

  def testProfileSampleUseFiltered(self):
    sys.platform = 'linux2'
    input_db = [
      {
        'command': r'clang -g -fprofile-sample-use=../path/to.prof -funroll-loops test.cc'
      }
    ]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(input_db, []),
      [{'command': r'clang -g -funroll-loops test.cc'}],
    )

  def testFilterArgs(self):
    sys.platform = 'linux2'
    input_db = [{'command': r'clang -g -ffile-compilation-dir=. -O3 test.cc'}]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(
        input_db,
        ['-ffile-compilation-dir=.', '-frandom-flag-that-does-not-exist'],
      ),
      [{'command': r'clang -g -O3 test.cc'}],
    )

  def testClangWrapperRemoved(self):
    sys.platform = 'linux2'
    # Test for clang_code_coverage_wrapper.py
    input_db = [
      {
        'command': r'python3 ../../build/toolchain/clang_code_coverage_wrapper.py '
        r'--target-os=linux ../../third_party/llvm-build/Release+Asserts/bin/clang++ '
        r'-O3 test.cc',
      }
    ]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(input_db, []),
      [
        {
          'command': r'../../third_party/llvm-build/Release+Asserts/bin/clang++ '
          r'-O3 test.cc'
        }
      ],
    )

    # Test for multiple versions and prefixes.
    input_db = [
      {
        'command': r'some_other_wrapper.py ./bin/clang-15 -O3 test.cc',
      },
      {
        'command': r'some_other_wrapper.py ./bin/clang++-15 -O3 test.cc',
      },
    ]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(input_db, []),
      [
        {'command': r'./bin/clang-15 -O3 test.cc'},
        {'command': r'./bin/clang++-15 -O3 test.cc'},
      ],
    )

    # Test for bare compiler (no wrapper).
    input_db = [
      {
        'command': r'clang++ -O3 test.cc',
      }
    ]
    self.assertEqual(
      compile_db.ProcessCompileDatabase(input_db, []),
      [{'command': r'clang++ -O3 test.cc'}],
    )

  @mock.patch('subprocess.check_output')
  def testGetInputDeps(self, mock_check_output):
    mock_check_output.return_value = '\n'.join(
      [
        '../../base/foo.cc',
        'gen/base/buildflags.h',
        'obj/base/base/foo.o',
        'obj/net/net.harddeps.stamp',
        'obj/net/net.inputdeps.stamp',
        'phony/base/base.harddeps',
        'phony/base/base.inputdeps',
      ]
    )

    deps = compile_db.GetInputDeps(
      'out/Default', targets=['chrome'], ninja_path='/path/to/ninja'
    )
    self.assertEqual(
      deps,
      [
        'obj/net/net.harddeps.stamp',
        'obj/net/net.inputdeps.stamp',
        'phony/base/base.harddeps',
        'phony/base/base.inputdeps',
      ],
    )
    mock_check_output.assert_called_once_with(
      ['/path/to/ninja', '-C', 'out/Default', '-t', 'inputs', 'chrome'],
      text=True,
    )

  @mock.patch('compile_db.GetAutoninjaPath', return_value='/path/to/autoninja')
  @mock.patch('compile_db.GetNinjaPath', return_value='/path/to/ninja')
  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('subprocess.check_call')
  @mock.patch('subprocess.check_output')
  def testBuildHeadersWithTargets(
    self,
    mock_check_output,
    mock_check_call,
    _mock_exists,
    _mock_ninja_path,
    _mock_autoninja_path,
  ):
    mock_check_output.return_value = (
      '../../base/foo.cc\nphony/base/base.inputdeps\n'
    )

    compile_db.BuildHeaders('out/Default', targets=['cast_browser_apk'])

    mock_check_output.assert_called_once_with(
      [
        '/path/to/ninja',
        '-C',
        'out/Default',
        '-t',
        'inputs',
        'cast_browser_apk',
      ],
      text=True,
    )
    mock_check_call.assert_called_once_with(
      ['/path/to/autoninja', '-C', 'out/Default', 'phony/base/base.inputdeps'],
      stdout=sys.stderr,
    )

  @mock.patch('compile_db.GetAutoninjaPath', return_value='/path/to/autoninja')
  @mock.patch('compile_db.GetNinjaPath', return_value='/path/to/ninja')
  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('subprocess.check_call')
  @mock.patch('subprocess.check_output')
  def testBuildHeadersWithoutTargets(
    self,
    mock_check_output,
    mock_check_call,
    _mock_exists,
    _mock_ninja_path,
    _mock_autoninja_path,
  ):
    mock_check_output.return_value = 'phony/base/base.inputdeps\n'

    compile_db.BuildHeaders('out/Default')

    mock_check_output.assert_called_once_with(
      ['/path/to/ninja', '-C', 'out/Default', '-t', 'inputs'],
      text=True,
    )
    mock_check_call.assert_called_once_with(
      ['/path/to/autoninja', '-C', 'out/Default', 'phony/base/base.inputdeps'],
      stdout=sys.stderr,
    )

  @mock.patch('compile_db.GetAutoninjaPath', return_value='/path/to/autoninja')
  @mock.patch('subprocess.check_call')
  def testBuildTargets_SplitsLongCommandLines(
    self, mock_check_call, _mock_autoninja_path
  ):
    sys.platform = 'win32'
    targets = [f'phony/target_{i:04d}.inputdeps' for i in range(400)]
    compile_db._BuildTargets('out/Default', targets)

    self.assertGreater(mock_check_call.call_count, 1)
    reconstructed = []
    for call in mock_check_call.call_args_list:
      cmd = call.args[0]
      self.assertEqual(cmd[:3], ['/path/to/autoninja', '-C', 'out/Default'])
      self.assertEqual(call.kwargs.get('stdout'), sys.stderr)
      reconstructed.extend(cmd[3:])
    self.assertEqual(reconstructed, targets)

  def testBuildHeaders_HermeticGnIntegration(self):
    gn_path = _GetGnPath()
    ninja_path = compile_db.GetNinjaPath()
    if not os.path.exists(gn_path) or not os.path.exists(ninja_path):
      self.skipTest('gn or ninja binary not found in checkout')

    with tempfile.TemporaryDirectory() as tmpdir:
      with open(os.path.join(tmpdir, '.gn'), 'w', encoding='utf-8') as f:
        f.write('buildconfig = "//BUILDCONFIG.gn"\n')
        f.write('no_stamp_files = true\n')

      with open(
        os.path.join(tmpdir, 'BUILDCONFIG.gn'), 'w', encoding='utf-8'
      ) as f:
        f.write('set_default_toolchain("//:default")\n')

      with open(
        os.path.join(tmpdir, 'gen_header.py'), 'w', encoding='utf-8'
      ) as f:
        f.write(
          'import sys\n'
          'with open(sys.argv[1], "w") as f:\n'
          '  f.write("#define GENERATED_VAL 1\\n")\n'
        )

      with open(os.path.join(tmpdir, 'BUILD.gn'), 'w', encoding='utf-8') as f:
        f.write("""
toolchain("default") {
  tool("cxx") {
    depfile = "{{output}}.d"
    command = "clang++ -MMD -MF $depfile -I. -Igen -c {{source}} -o {{output}}"
    depsformat = "gcc"
    outputs = [
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o",
    ]
  }
  tool("alink") {
    command = "ar rcs {{output}} {{inputs}}"
    outputs = [ "{{target_out_dir}}/{{target_output_name}}.a" ]
  }
  tool("link") {
    command = "clang++ {{inputs}} -o {{output}}"
    outputs = [ "{{root_out_dir}}/{{target_output_name}}" ]
  }
  tool("stamp") {
    command = "touch {{output}}"
  }
  tool("copy") {
    command = "cp {{source}} {{output}}"
  }
}

action("gen_common_header") {
  script = "//gen_header.py"
  outputs = [ "$target_gen_dir/common_generated.h" ]
  args = [ rebase_path("$target_gen_dir/common_generated.h", root_build_dir) ]
}

action("gen_app_header") {
  script = "//gen_header.py"
  outputs = [ "$target_gen_dir/app_generated.h" ]
  args = [ rebase_path("$target_gen_dir/app_generated.h", root_build_dir) ]
}

executable("app") {
  sources = [ "main.cc", "helper.cc" ]
  deps = [ ":gen_common_header", ":gen_app_header" ]
}

action("gen_other_header") {
  script = "//gen_header.py"
  outputs = [ "$target_gen_dir/other_generated.h" ]
  args = [ rebase_path("$target_gen_dir/other_generated.h", root_build_dir) ]
}

executable("other_app") {
  sources = [ "other_main.cc", "other_helper.cc" ]
  deps = [ ":gen_common_header", ":gen_other_header" ]
}
""")

      for src in ('main.cc', 'helper.cc', 'other_main.cc', 'other_helper.cc'):
        with open(os.path.join(tmpdir, src), 'w', encoding='utf-8') as f:
          f.write('int x = 0;\n')

      out_dir = os.path.join(tmpdir, 'out')
      subprocess.check_call(
        [
          gn_path,
          'gen',
          out_dir,
          f'--root={tmpdir}',
          f'--script-executable={sys.executable}',
        ]
      )

      app_header = os.path.join(out_dir, 'gen', 'app_generated.h')
      other_header = os.path.join(out_dir, 'gen', 'other_generated.h')
      main_obj = os.path.join(out_dir, 'obj', 'app.main.o')
      helper_obj = os.path.join(out_dir, 'obj', 'app.helper.o')

      self.assertIn(
        compile_db.GetInputDeps(
          out_dir, targets=['app'], ninja_path=ninja_path
        ),
        (['phony/app.inputdeps'], ['phony/app.harddeps']),
      )
      self.assertFalse(os.path.exists(app_header))
      self.assertFalse(os.path.exists(other_header))

      with mock.patch('compile_db.GetAutoninjaPath', return_value=ninja_path):
        compile_db.BuildHeaders(out_dir, targets=['app'])
        compdb = compile_db.GenerateWithNinja(out_dir, targets=['app'])

      # Target-scoped BuildHeaders should build app's generated header, skip
      # other_app's generated header, and skip compiling C++ object files.
      self.assertTrue(os.path.exists(app_header))
      self.assertFalse(os.path.exists(other_header))
      self.assertFalse(os.path.exists(main_obj))
      self.assertFalse(os.path.exists(helper_obj))
      self.assertEqual(len(compdb), 4)

      with mock.patch('compile_db.GetAutoninjaPath', return_value=ninja_path):
        compile_db.BuildHeaders(out_dir)

      # Unscoped BuildHeaders should build all generated headers across the
      # default build graph while still skipping C++ object compilation.
      self.assertTrue(os.path.exists(other_header))
      self.assertFalse(os.path.exists(main_obj))


if __name__ == '__main__':
  unittest.main()
