#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for mb.py."""

from __future__ import print_function

import json
import os
import re
import StringIO
import sys
import unittest

import mb


class FakeMBW(mb.MetaBuildWrapper):
  def __init__(self, win32=False):
    super(FakeMBW, self).__init__()

    # Override vars for test portability.
    if win32:
      self.chromium_src_dir = 'c:\\fake_src'
      self.default_config = 'c:\\fake_src\\tools\\mb\\mb_config.pyl'
      self.default_isolate_map = ('c:\\fake_src\\testing\\buildbot\\'
                                  'gn_isolate_map.pyl')
      self.platform = 'win32'
      self.executable = 'c:\\python\\python.exe'
      self.sep = '\\'
      self.cwd = 'c:\\fake_src\\out\\Default'
    else:
      self.chromium_src_dir = '/fake_src'
      self.default_config = '/fake_src/tools/mb/mb_config.pyl'
      self.default_isolate_map = '/fake_src/testing/buildbot/gn_isolate_map.pyl'
      self.executable = '/usr/bin/python'
      self.platform = 'linux2'
      self.sep = '/'
      self.cwd = '/fake_src/out/Default'

    self.files = {}
    self.calls = []
    self.cmds = []
    self.cross_compile = None
    self.out = ''
    self.err = ''
    self.rmdirs = []

  def ExpandUser(self, path):
    return '$HOME/%s' % path

  def Exists(self, path):
    return self.files.get(self._AbsPath(path)) is not None

  def MaybeMakeDirectory(self, path):
    abpath = self._AbsPath(path)
    self.files[abpath] = True

  def PathJoin(self, *comps):
    return self.sep.join(comps)

  def ReadFile(self, path):
    return self.files[self._AbsPath(path)]

  def WriteFile(self, path, contents, force_verbose=False):
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    abpath = self._AbsPath(path)
    self.files[abpath] = contents

  def Call(self, cmd, env=None, buffer_output=True, stdin=None):
    self.calls.append(cmd)
    if self.cmds:
      return self.cmds.pop(0)
    return 0, '', ''

  def Print(self, *args, **kwargs):
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')
    f = kwargs.get('file', sys.stdout)
    if f == sys.stderr:
      self.err += sep.join(args) + end
    else:
      self.out += sep.join(args) + end

  def TempFile(self, mode='w'):
    return FakeFile(self.files)

  def RemoveFile(self, path):
    abpath = self._AbsPath(path)
    self.files[abpath] = None

  def RemoveDirectory(self, path):
    abpath = self._AbsPath(path)
    self.rmdirs.append(abpath)
    files_to_delete = [f for f in self.files if f.startswith(abpath)]
    for f in files_to_delete:
      self.files[f] = None

  def _AbsPath(self, path):
    if not ((self.platform == 'win32' and path.startswith('c:')) or
            (self.platform != 'win32' and path.startswith('/'))):
      path = self.PathJoin(self.cwd, path)
    if self.sep == '\\':
      return re.sub(r'\\+', r'\\', path)
    else:
      return re.sub('/+', '/', path)


class FakeFile(object):
  def __init__(self, files):
    self.name = '/tmp/file'
    self.buf = ''
    self.files = files

  def write(self, contents):
    self.buf += contents

  def close(self):
     self.files[self.name] = self.buf


TEST_CONFIG = """\
{
  'masters': {
    'chromium': {},
    'fake_master': {
      'fake_builder': 'rel_bot',
      'fake_debug_builder': 'debug_goma',
      'fake_simplechrome_builder': 'cros_chrome_sdk',
      'fake_args_bot': '//build/args/bots/fake_master/fake_args_bot.gn',
      'fake_multi_phase': { 'phase_1': 'phase_1', 'phase_2': 'phase_2'},
      'fake_args_file': 'args_file_goma',
      'fake_args_file_twice': 'args_file_twice',
    },
  },
  'configs': {
    'args_file_goma': ['args_file', 'goma'],
    'args_file_twice': ['args_file', 'args_file'],
    'cros_chrome_sdk': ['cros_chrome_sdk'],
    'rel_bot': ['rel', 'goma', 'fake_feature1'],
    'debug_goma': ['debug', 'goma'],
    'phase_1': ['phase_1'],
    'phase_2': ['phase_2'],
  },
  'mixins': {
    'cros_chrome_sdk': {
      'cros_passthrough': True,
    },
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
    },
    'goma': {
      'gn_args': 'use_goma=true',
    },
    'args_file': {
      'args_file': '//build/args/fake.gn',
    },
    'phase_1': {
      'gn_args': 'phase=1',
    },
    'phase_2': {
      'gn_args': 'phase=2',
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
  },
}
"""


TEST_BAD_CONFIG = """\
{
  'configs': {
    'rel_bot_1': ['rel', 'chrome_with_codecs'],
    'rel_bot_2': ['rel', 'bad_nested_config'],
  },
  'masters': {
    'chromium': {
      'a': 'rel_bot_1',
      'b': 'rel_bot_2',
    },
  },
  'mixins': {
    'chrome_with_codecs': {
      'gn_args': 'proprietary_codecs=true',
    },
    'bad_nested_config': {
      'mixins': ['chrome_with_codecs'],
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
  },
}
"""

TRYSERVER_CONFIG = """\
{
  'masters': {
    'not_a_tryserver': {
      'fake_builder': 'fake_config',
    },
    'tryserver.chromium.linux': {
      'try_builder': 'fake_config',
    },
    'tryserver.chromium.mac': {
      'try_builder2': 'fake_config',
    },
  },
  'configs': {},
  'mixins': {},
}
"""


class UnitTest(unittest.TestCase):
  def fake_mbw(self, files=None, win32=False):
    mbw = FakeMBW(win32=win32)
    mbw.files.setdefault(mbw.default_config, TEST_CONFIG)
    mbw.files.setdefault(
      mbw.ToAbsPath('//testing/buildbot/gn_isolate_map.pyl'),
      '''{
        "foo_unittests": {
          "label": "//foo:foo_unittests",
          "type": "console_test_launcher",
          "args": [],
        },
      }''')
    mbw.files.setdefault(
        mbw.ToAbsPath('//build/args/bots/fake_master/fake_args_bot.gn'),
        'is_debug = false\n')
    if files:
      for path, contents in files.items():
        mbw.files[path] = contents
    return mbw

  def check(self, args, mbw=None, files=None, out=None, err=None, ret=None,
            env=None):
    if not mbw:
      mbw = self.fake_mbw(files)

    try:
      prev_env = os.environ.copy()
      os.environ = env if env else prev_env
      actual_ret = mbw.Main(args)
    finally:
      os.environ = prev_env

    self.assertEqual(actual_ret, ret)
    if out is not None:
      self.assertEqual(mbw.out, out)
    if err is not None:
      self.assertEqual(mbw.err, err)
    return mbw

  def test_analyze(self):
    files = {'/tmp/in.json': '''{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests"],
               "additional_compile_targets": ["all"]
             }''',
             '/tmp/out.json.gn': '''{\
               "status": "Found dependency",
               "compile_targets": ["//foo:foo_unittests"],
               "test_targets": ["//foo:foo_unittests"]
             }'''}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True, stdin=None: (0, '', '')

    self.check(['analyze', '-c', 'debug_goma', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency',
      'compile_targets': ['foo:foo_unittests'],
      'test_targets': ['foo_unittests']
    })

  def test_analyze_optimizes_compile_for_all(self):
    files = {'/tmp/in.json': '''{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests"],
               "additional_compile_targets": ["all"]
             }''',
             '/tmp/out.json.gn': '''{\
               "status": "Found dependency",
               "compile_targets": ["//foo:foo_unittests", "all"],
               "test_targets": ["//foo:foo_unittests"]
             }'''}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True, stdin=None: (0, '', '')

    self.check(['analyze', '-c', 'debug_goma', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])

    # check that 'foo_unittests' is not in the compile_targets
    self.assertEqual(['all'], out['compile_targets'])

  def test_analyze_handles_other_toolchains(self):
    files = {'/tmp/in.json': '''{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests"],
               "additional_compile_targets": ["all"]
             }''',
             '/tmp/out.json.gn': '''{\
               "status": "Found dependency",
               "compile_targets": ["//foo:foo_unittests",
                                   "//foo:foo_unittests(bar)"],
               "test_targets": ["//foo:foo_unittests"]
             }'''}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True, stdin=None: (0, '', '')

    self.check(['analyze', '-c', 'debug_goma', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])

    # crbug.com/736215: If GN returns a label containing a toolchain,
    # MB (and Ninja) don't know how to handle it; to work around this,
    # we give up and just build everything we were asked to build. The
    # output compile_targets should include all of the input test_targets and
    # additional_compile_targets.
    self.assertEqual(['all', 'foo_unittests'], out['compile_targets'])

  def test_analyze_handles_way_too_many_results(self):
    too_many_files = ', '.join(['"//foo:foo%d"' % i for i in xrange(4 * 1024)])
    files = {'/tmp/in.json': '''{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests"],
               "additional_compile_targets": ["all"]
             }''',
             '/tmp/out.json.gn': '''{\
               "status": "Found dependency",
               "compile_targets": [''' + too_many_files + '''],
               "test_targets": ["//foo:foo_unittests"]
             }'''}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True, stdin=None: (0, '', '')

    self.check(['analyze', '-c', 'debug_goma', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])

    # If GN returns so many compile targets that we might have command-line
    # issues, we should give up and just build everything we were asked to
    # build. The output compile_targets should include all of the input
    # test_targets and additional_compile_targets.
    self.assertEqual(['all', 'foo_unittests'], out['compile_targets'])

  def test_gen(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-c', 'debug_goma', '//out/Default', '-g', '/goma'],
               mbw=mbw, ret=0)
    self.assertMultiLineEqual(mbw.files['/fake_src/out/Default/args.gn'],
                              ('goma_dir = "/goma"\n'
                               'is_debug = true\n'
                               'use_goma = true\n'))

    # Make sure we log both what is written to args.gn and the command line.
    self.assertIn('Writing """', mbw.out)
    self.assertIn('/fake_src/buildtools/linux64/gn gen //out/Default --check',
                  mbw.out)

    mbw = self.fake_mbw(win32=True)
    self.check(['gen', '-c', 'debug_goma', '-g', 'c:\\goma', '//out/Debug'],
               mbw=mbw, ret=0)
    self.assertMultiLineEqual(mbw.files['c:\\fake_src\\out\\Debug\\args.gn'],
                              ('goma_dir = "c:\\\\goma"\n'
                               'is_debug = true\n'
                               'use_goma = true\n'))
    self.assertIn('c:\\fake_src\\buildtools\\win\\gn.exe gen //out/Debug '
                  '--check\n', mbw.out)

    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_master', '-b', 'fake_args_bot',
                '//out/Debug'],
               mbw=mbw, ret=0)
    self.assertEqual(
        mbw.files['/fake_src/out/Debug/args.gn'],
        'import("//build/args/bots/fake_master/fake_args_bot.gn")\n')

  def test_gen_args_file_mixins(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_master', '-b', 'fake_args_file',
                '//out/Debug'], mbw=mbw, ret=0)

    self.assertEqual(
        mbw.files['/fake_src/out/Debug/args.gn'],
        ('import("//build/args/fake.gn")\n'
         'use_goma = true\n'))

    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_master', '-b', 'fake_args_file_twice',
                '//out/Debug'], mbw=mbw, ret=1)

  def test_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd, env=None, buffer_output=True, stdin=None: (1, '', '')
    self.check(['gen', '-c', 'debug_goma', '//out/Default'], mbw=mbw, ret=1)

  def test_gen_swarming(self):
    files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
    }

    mbw = self.fake_mbw(files)

    def fake_call(cmd, env=None, buffer_output=True, stdin=None):
      del cmd
      del env
      del buffer_output
      del stdin
      mbw.files['/fake_src/out/Default/base_unittests.runtime_deps'] = (
          'base_unittests\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/base_unittests.isolate',
                  mbw.files)
    self.assertIn('/fake_src/out/Default/base_unittests.isolated.gen.json',
                  mbw.files)

  def test_gen_swarming_script(self):
    files = {
      '/tmp/swarming_targets': 'cc_perftests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'cc_perftests': {"
          "  'label': '//cc:cc_perftests',"
          "  'type': 'script',"
          "  'script': '/fake_src/out/Default/test_script.py',"
          "  'args': [],"
          "}}\n"
      ),
    }
    mbw = self.fake_mbw(files=files)

    def fake_call(cmd, env=None, buffer_output=True, stdin=None):
      del cmd
      del env
      del buffer_output
      del stdin
      mbw.files['/fake_src/out/Default/cc_perftests.runtime_deps'] = (
          'cc_perftests\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map.pyl',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolate',
                  mbw.files)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolated.gen.json',
                  mbw.files)

  def test_gen_fuzzer(self):
    files = {
      '/tmp/swarming_targets': 'cc_perftests_fuzzer\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'cc_perftests_fuzzer': {"
          "  'label': '//cc:cc_perftests_fuzzer',"
          "  'type': 'fuzzer',"
          "}}\n"
      ),
    }

    mbw = self.fake_mbw(files=files)

    def fake_call(cmd, env=None, buffer_output=True, stdin=None):
      del cmd
      del env
      del buffer_output
      del stdin
      mbw.files['/fake_src/out/Default/cc_perftests_fuzzer.runtime_deps'] = (
          'cc_perftests_fuzzer\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map.pyl',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/cc_perftests_fuzzer.isolate',
                  mbw.files)
    self.assertIn(
        '/fake_src/out/Default/cc_perftests_fuzzer.isolated.gen.json',
        mbw.files)

  def test_multiple_isolate_maps(self):
    files = {
      '/tmp/swarming_targets': 'cc_perftests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'cc_perftests': {"
          "  'label': '//cc:cc_perftests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/testing/buildbot/gn_isolate_map2.pyl': (
          "{'cc_perftests2': {"
          "  'label': '//cc:cc_perftests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
    }
    mbw = self.fake_mbw(files=files)

    def fake_call(cmd, env=None, buffer_output=True, stdin=None):
      del cmd
      del env
      del buffer_output
      del stdin
      mbw.files['/fake_src/out/Default/cc_perftests.runtime_deps'] = (
          'cc_perftests_fuzzer\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map.pyl',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map2.pyl',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolate',
                  mbw.files)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolated.gen.json',
                  mbw.files)


  def test_duplicate_isolate_maps(self):
    files = {
      '/tmp/swarming_targets': 'cc_perftests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'cc_perftests': {"
          "  'label': '//cc:cc_perftests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/testing/buildbot/gn_isolate_map2.pyl': (
          "{'cc_perftests': {"
          "  'label': '//cc:cc_perftests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      'c:\\fake_src\out\Default\cc_perftests.exe.runtime_deps': (
          "cc_perftests\n"
      ),
    }
    mbw = self.fake_mbw(files=files, win32=True)
    # Check that passing duplicate targets into mb fails.
    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map.pyl',
                '--isolate-map-file',
                '/fake_src/testing/buildbot/gn_isolate_map2.pyl',
                '//out/Default'], mbw=mbw, ret=1)


  def test_isolate(self):
    files = {
      '/fake_src/out/Default/toolchain.ninja': "",
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['isolate', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], files=files, ret=0)

    # test running isolate on an existing build_dir
    files['/fake_src/out/Default/args.gn'] = 'is_debug = True\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

  def test_isolate_dir(self):
    files = {
      '/fake_src/out/Default/toolchain.ninja': "",
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
    }
    mbw = self.fake_mbw(files=files)
    mbw.cmds.append((0, '', ''))  # Result of `gn gen`
    mbw.cmds.append((0, '', ''))  # Result of `autoninja`

    # Result of `gn desc runtime_deps`
    mbw.cmds.append((0, 'base_unitests\n../../test_data/\n', ''))
    self.check(['isolate', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], mbw=mbw, ret=0, err='')

  def test_isolate_generated_dir(self):
    files = {
      '/fake_src/out/Default/toolchain.ninja': "",
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
    }
    mbw = self.fake_mbw(files=files)
    mbw.cmds.append((0, '', ''))  # Result of `gn gen`
    mbw.cmds.append((0, '', ''))  # Result of `autoninja`

    # Result of `gn desc runtime_deps`
    mbw.cmds.append((0, 'base_unitests\ntest_data/\n', ''))
    expected_err = ('error: gn `data` items may not list generated directories;'
                    ' list files in directory instead for:\n'
                    '//out/Default/test_data/\n')
    self.check(['isolate', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], mbw=mbw, ret=1)
    self.assertEqual(mbw.out[-len(expected_err):], expected_err)


  def test_run(self):
    files = {
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['run', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], files=files, ret=0)

  def test_run_swarmed(self):
    files = {
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }

    def run_stub(cmd, **_kwargs):
      if 'isolate.py' in cmd[1]:
        return 0, 'fake_hash base_unittests', ''
      else:
        return 0, '', ''

    mbw = self.fake_mbw(files=files)
    mbw.Run = run_stub
    self.check(['run', '-s', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], mbw=mbw, ret=0)
    self.check(['run', '-s', '-c', 'debug_goma', '-d', 'os', 'Win7',
                '//out/Default', 'base_unittests'], mbw=mbw, ret=0)

  def test_lookup(self):
    self.check(['lookup', '-c', 'debug_goma'], ret=0,
               out=('\n'
                    'Writing """\\\n'
                    'is_debug = true\n'
                    'use_goma = true\n'
                    '""" to _path_/args.gn.\n\n'
                    '/fake_src/buildtools/linux64/gn gen _path_\n'))

  def test_quiet_lookup(self):
    self.check(['lookup', '-c', 'debug_goma', '--quiet'], ret=0,
               out=('is_debug = true\n'
                    'use_goma = true\n'))

  def test_lookup_goma_dir_expansion(self):
    self.check(['lookup', '-c', 'rel_bot', '-g', '/foo'], ret=0,
               out=('\n'
                    'Writing """\\\n'
                    'enable_doom_melon = true\n'
                    'goma_dir = "/foo"\n'
                    'is_debug = false\n'
                    'use_goma = true\n'
                    '""" to _path_/args.gn.\n\n'
                    '/fake_src/buildtools/linux64/gn gen _path_\n'))

  def test_lookup_simplechrome(self):
    simplechrome_env = {
        'GN_ARGS': 'is_chromeos=1 target_os="chromeos"',
    }
    self.check(['lookup', '-c', 'cros_chrome_sdk'], ret=0, env=simplechrome_env)

  def test_help(self):
    orig_stdout = sys.stdout
    try:
      sys.stdout = StringIO.StringIO()
      self.assertRaises(SystemExit, self.check, ['-h'])
      self.assertRaises(SystemExit, self.check, ['help'])
      self.assertRaises(SystemExit, self.check, ['help', 'gen'])
    finally:
      sys.stdout = orig_stdout

  def test_multiple_phases(self):
    # Check that not passing a --phase to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_master', '-b', 'fake_multi_phase'],
                     ret=1)
    self.assertIn('Must specify a build --phase', mbw.out)

    # Check that passing a --phase to a single-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_master', '-b', 'fake_builder',
                      '--phase', 'phase_1'], ret=1)
    self.assertIn('Must not specify a build --phase', mbw.out)

    # Check that passing a wrong phase key to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_master', '-b', 'fake_multi_phase',
                      '--phase', 'wrong_phase'], ret=1)
    self.assertIn('Phase wrong_phase doesn\'t exist', mbw.out)

    # Check that passing a correct phase key to a multi-phase builder passes.
    mbw = self.check(['lookup', '-m', 'fake_master', '-b', 'fake_multi_phase',
                      '--phase', 'phase_1'], ret=0)
    self.assertIn('phase = 1', mbw.out)

    mbw = self.check(['lookup', '-m', 'fake_master', '-b', 'fake_multi_phase',
                      '--phase', 'phase_2'], ret=0)
    self.assertIn('phase = 2', mbw.out)

  def test_recursive_lookup(self):
    files = {
        '/fake_src/build/args/fake.gn': (
          'enable_doom_melon = true\n'
          'enable_antidoom_banana = true\n'
        )
    }
    self.check(['lookup', '-m', 'fake_master', '-b', 'fake_args_file',
                '--recursive'], files=files, ret=0,
               out=('enable_antidoom_banana = true\n'
                    'enable_doom_melon = true\n'
                    'use_goma = true\n'))

  def test_validate(self):
    mbw = self.fake_mbw()
    self.check(['validate'], mbw=mbw, ret=0)

  def test_bad_validate(self):
    mbw = self.fake_mbw()
    mbw.files[mbw.default_config] = TEST_BAD_CONFIG
    self.check(['validate'], mbw=mbw, ret=1)

  def test_build_command_unix(self):
    files = {
      '/fake_src/out/Default/toolchain.ninja': '',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          '{"base_unittests": {'
          '  "label": "//base:base_unittests",'
          '  "type": "raw",'
          '  "args": [],'
          '}}\n')
    }

    mbw = self.fake_mbw(files)
    self.check(['run', '//out/Default', 'base_unittests'], mbw=mbw, ret=0)
    self.assertIn(['autoninja', '-C', 'out/Default', 'base_unittests'],
                  mbw.calls)

  def test_build_command_windows(self):
    files = {
      'c:\\fake_src\\out\\Default\\toolchain.ninja': '',
      'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl': (
          '{"base_unittests": {'
          '  "label": "//base:base_unittests",'
          '  "type": "raw",'
          '  "args": [],'
          '}}\n')
    }

    mbw = self.fake_mbw(files, True)
    self.check(['run', '//out/Default', 'base_unittests'], mbw=mbw, ret=0)
    self.assertIn(['autoninja.bat', '-C', 'out\\Default', 'base_unittests'],
                  mbw.calls)


if __name__ == '__main__':
  unittest.main()
