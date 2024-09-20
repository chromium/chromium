#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for mb.py."""

import io
import json
import os
import re
import sys
import textwrap
import unittest

sys.path.insert(
    0,
    os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')))

from mb import mb

# Call has argument input to match subprocess.run
# pylint: disable=redefined-builtin


class FakeMBW(mb.MetaBuildWrapper):
  def __init__(self, win32=False):
    super().__init__()

    # Override vars for test portability.
    if win32:
      self.chromium_src_dir = 'c:\\fake_src'
      self.default_config = 'c:\\fake_src\\tools\\mb\\mb_config.pyl'
      self.default_isolate_map = ('c:\\fake_src\\testing\\buildbot\\'
                                  'gn_isolate_map.pyl')
      self.temp = 'c:\\temp'
      self.platform = 'win32'
      self.executable = 'c:\\python\\python.exe'
      self.sep = '\\'
      self.cwd = 'c:\\fake_src\\out\\Default'
    else:
      self.chromium_src_dir = '/fake_src'
      self.default_config = '/fake_src/tools/mb/mb_config.pyl'
      self.default_isolate_map = '/fake_src/testing/buildbot/gn_isolate_map.pyl'
      self.temp = '/tmp'
      self.platform = 'linux'
      self.executable = '/usr/bin/python'
      self.sep = '/'
      self.cwd = '/fake_src/out/Default'

    self.files = {}
    self.dirs = set()
    self.calls = []
    self.cmds = []
    self.cross_compile = None
    self.out = ''
    self.err = ''
    self.rmdirs = []

  def Exists(self, path):
    abs_path = self._AbsPath(path)
    return (self.files.get(abs_path) is not None or abs_path in self.dirs)

  def ListDir(self, path):
    dir_contents = []
    for f in list(self.files.keys()) + list(self.dirs):
      head, _ = os.path.split(f)
      if head == path:
        dir_contents.append(f)
    return dir_contents

  def MaybeMakeDirectory(self, path):
    abpath = self._AbsPath(path)
    self.dirs.add(abpath)

  def PathJoin(self, *comps):
    return self.sep.join(comps)

  def ReadFile(self, path):
    try:
      return self.files[self._AbsPath(path)]
    except KeyError as e:
      raise IOError('%s not found' % path) from e

  def WriteFile(self, path, contents, force_verbose=False):
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    abpath = self._AbsPath(path)
    self.files[abpath] = contents

  def Call(self, cmd, env=None, capture_output=True, input=None):
    # Avoid unused-argument warnings from Pylint
    del env
    del capture_output
    del input
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

  def TempDir(self):
    tmp_dir = self.temp + self.sep + 'mb_test'
    self.dirs.add(tmp_dir)
    return tmp_dir

  def TempFile(self, mode='w'):
    # Avoid unused-argument warnings from Pylint
    del mode
    return FakeFile(self.files)

  def RemoveFile(self, path):
    abpath = self._AbsPath(path)
    self.files[abpath] = None

  def RemoveDirectory(self, abs_path):
    # Normalize the passed-in path to handle different working directories
    # used during unit testing.
    abs_path = self._AbsPath(abs_path)
    self.rmdirs.append(abs_path)
    files_to_delete = [f for f in self.files if f.startswith(abs_path)]
    for f in files_to_delete:
      self.files[f] = None

  def _AbsPath(self, path):
    if not ((self.platform == 'win32' and path.startswith('c:')) or
            (self.platform != 'win32' and path.startswith('/'))):
      path = self.PathJoin(self.cwd, path)
    if self.sep == '\\':
      return re.sub(r'\\+', r'\\', path)
    return re.sub('/+', '/', path)


class FakeFile:
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
  'builder_groups': {
    'chromium': {},
    'fake_builder_group': {
      'fake_args_bot': 'fake_args_bot',
      'fake_args_file': 'args_file_remoteexec',
      'fake_builder': 'rel_bot',
      'fake_debug_builder': 'debug_remoteexec',
      'fake_multi_phase': { 'phase_1': 'phase_1', 'phase_2': 'phase_2'},
    },
  },
  'configs': {
    'args_file_remoteexec': ['fake_args_bot', 'remoteexec'],
    'debug_remoteexec': ['debug', 'remoteexec'],
    'fake_args_bot': ['fake_args_bot'],
    'phase_1': ['rel', 'phase_1'],
    'phase_2': ['rel', 'phase_2'],
    'rel_bot': ['rel', 'remoteexec', 'fake_feature1'],
  },
  'mixins': {
    'debug': {
      'gn_args': 'is_debug=true',
    },
    'fake_args_bot': {
      'args_file': '//build/args/bots/fake_builder_group/fake_args_bot.gn',
    },
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
    },
    'phase_1': {
      'gn_args': 'phase=1',
    },
    'phase_2': {
      'gn_args': 'phase=2',
    },
    'rel': {
      'gn_args': 'is_debug=false dcheck_always_on=false',
    },
    'remoteexec': {
      'gn_args': 'use_remoteexec=true',
    },
  },
}
"""

CONFIG_STARLARK_GN_ARGS = """\
{
  'gn_args_locations_files': [
      '../../infra/config/generated/builders/gn_args_locations.json',
  ],
  'builder_groups': {
  },
  'configs': {
  },
  'mixins': {
  },
}
"""

TEST_GN_ARGS_LOCATIONS_JSON = """\
{
  "chromium": {
    "linux-official": "ci/linux-official/gn-args.json"
  },
  "tryserver.chromium": {
    "linux-official": "try/linux-official/gn-args.json"
  }
}
"""

TEST_GN_ARGS_JSON = """\
{
  "gn_args": {
    "string_arg": "has double quotes",
    "bool_arg_lower_case": true,
    "string_list_arg": ["foo", "bar", "baz"],
    "dict_arg": {
      "string": "foo",
      "bool": true,
      "list": ["foo", "bar", "baz"]
    }
  }
}
"""

TEST_PHASED_GN_ARGS_JSON = """\
{
  "phases": {
    "phase_1": {
      "gn_args": {
        "string_arg": "has double quotes",
        "bool_arg_lower_case": true
      }
    },
    "phase_2": {
      "gn_args": {
        "string_arg": "second phase",
        "bool_arg_lower_case": false
      }
    }
  }
}
"""

TEST_BAD_CONFIG = """\
{
  'configs': {
    'rel_bot_1': ['rel', 'chrome_with_codecs'],
    'rel_bot_2': ['rel', 'bad_nested_config'],
  },
  'builder_groups': {
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



TEST_ARGS_FILE_TWICE_CONFIG = """\
{
  'builder_groups': {
    'chromium': {},
    'fake_builder_group': {
      'fake_args_file_twice': 'args_file_twice',
    },
  },
  'configs': {
    'args_file_twice': ['args_file', 'args_file'],
  },
  'mixins': {
    'args_file': {
      'args_file': '//build/args/fake.gn',
    },
  },
}
"""


TEST_DUP_CONFIG = """\
{
  'builder_groups': {
    'chromium': {},
    'fake_builder_group': {
      'fake_builder': 'some_config',
      'other_builder': 'some_other_config',
    },
  },
  'configs': {
    'some_config': ['args_file'],
    'some_other_config': ['args_file'],
  },
  'mixins': {
    'args_file': {
      'args_file': '//build/args/fake.gn',
    },
  },
}
"""

TRYSERVER_CONFIG = """\
{
  'builder_groups': {
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


def is_win():
  return sys.platform == 'win32'


class UnitTest(unittest.TestCase):
  maxDiff = None

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
        mbw.ToAbsPath('//build/args/bots/fake_builder_group/fake_args_bot.gn'),
        'is_debug = false\ndcheck_always_on=false\n')
    mbw.files.setdefault(mbw.ToAbsPath('//tools/mb/rts_banned_suites.json'),
                         '{}')
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
      if env:
        os.environ.clear()
        os.environ.update(env)
      actual_ret = mbw.Main(args)
    finally:
      os.environ.clear()
      os.environ.update(prev_env)
    self.assertEqual(
        actual_ret, ret,
        "ret: %s, out: %s, err: %s" % (actual_ret, mbw.out, mbw.err))
    if out is not None:
      self.assertEqual(mbw.out, out)
    if err is not None:
      self.assertEqual(mbw.err, err)
    return mbw

  def path(self, p):
    if is_win():
      return 'c:' + p.replace('/', '\\')
    return p

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
    mbw.Call = lambda cmd, env=None, capture_output=True, input='': (0, '', '')

    self.check([
        'analyze', '-c', 'debug_remoteexec', '//out/Default', '/tmp/in.json',
        '/tmp/out.json'
    ],
               mbw=mbw,
               ret=0)
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
    mbw.Call = lambda cmd, env=None, capture_output=True, input='': (0, '', '')

    self.check([
        'analyze', '-c', 'debug_remoteexec', '//out/Default', '/tmp/in.json',
        '/tmp/out.json'
    ],
               mbw=mbw,
               ret=0)
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
    mbw.Call = lambda cmd, env=None, capture_output=True, input='': (0, '', '')

    self.check([
        'analyze', '-c', 'debug_remoteexec', '//out/Default', '/tmp/in.json',
        '/tmp/out.json'
    ],
               mbw=mbw,
               ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])

    # crbug.com/736215: If GN returns a label containing a toolchain,
    # MB (and Ninja) don't know how to handle it; to work around this,
    # we give up and just build everything we were asked to build. The
    # output compile_targets should include all of the input test_targets and
    # additional_compile_targets.
    self.assertEqual(['all', 'foo_unittests'], out['compile_targets'])

  def test_analyze_handles_way_too_many_results(self):
    too_many_files = ', '.join(['"//foo:foo%d"' % i for i in range(40 * 1024)])
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
    mbw.Call = lambda cmd, env=None, capture_output=True, input='': (0, '', '')

    self.check([
        'analyze', '-c', 'debug_remoteexec', '//out/Default', '/tmp/in.json',
        '/tmp/out.json'
    ],
               mbw=mbw,
               ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])

    # If GN returns so many compile targets that we might have command-line
    # issues, we should give up and just build everything we were asked to
    # build. The output compile_targets should include all of the input
    # test_targets and additional_compile_targets.
    self.assertEqual(['all', 'foo_unittests'], out['compile_targets'])

  def test_gen(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-c', 'debug_remoteexec', '//out/Default'],
               mbw=mbw,
               ret=0)
    self.assertMultiLineEqual(mbw.files['/fake_src/out/Default/args.gn'],
                              ('is_debug = true\n'
                               'use_remoteexec = true\n'))

    # Make sure we log both what is written to args.gn and the command line.
    self.assertIn('Writing """', mbw.out)
    self.assertIn('/fake_src/buildtools/linux64/gn gen //out/Default --check',
                  mbw.out)

    mbw = self.fake_mbw(win32=True)
    self.check(['gen', '-c', 'debug_remoteexec', '//out/Debug'], mbw=mbw, ret=0)
    self.assertMultiLineEqual(mbw.files['c:\\fake_src\\out\\Debug\\args.gn'],
                              ('is_debug = true\n'
                               'use_remoteexec = true\n'))
    self.assertIn(
        'c:\\fake_src\\buildtools\\win\\gn.exe gen //out/Debug '
        '--check', mbw.out)

    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_builder_group', '-b', 'fake_args_bot',
                '//out/Debug'],
               mbw=mbw, ret=0)
    # TODO(crbug.com/40134852): This assert is inappropriately failing.
    # self.assertEqual(
    #     mbw.files['/fake_src/out/Debug/args.gn'],
    #     'import("//build/args/bots/fake_builder_group/fake_args_bot.gn")\n')

  def test_gen_args_file_mixins(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_builder_group', '-b', 'fake_args_file',
                '//out/Debug'], mbw=mbw, ret=0)

    self.assertEqual(
        mbw.files['/fake_src/out/Debug/args.gn'],
        ('import("//build/args/bots/fake_builder_group/fake_args_bot.gn")\n'
         'use_remoteexec = true\n'))

  def test_gen_args_file_twice(self):
    mbw = self.fake_mbw()
    mbw.files[mbw.default_config] = TEST_ARGS_FILE_TWICE_CONFIG
    self.check(['gen', '-m', 'fake_builder_group', '-b', 'fake_args_file_twice',
                '//out/Debug'], mbw=mbw, ret=1)

  def test_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd, env=None, capture_output=True, input='': (1, '', '')
    self.check(['gen', '-c', 'debug_remoteexec', '//out/Default'],
               mbw=mbw,
               ret=1)

  def test_gen_swarming(self):
    files = {
        '/tmp/swarming_targets':
        'base_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }

    mbw = self.fake_mbw(files)

    def fake_call(cmd, env=None, capture_output=True, input=''):
      del cmd
      del env
      del capture_output
      del input
      mbw.files['/fake_src/out/Default/base_unittests.runtime_deps'] = (
          'base_unittests\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '//out/Default'
    ],
               mbw=mbw,
               ret=0)
    self.assertIn('/fake_src/out/Default/base_unittests.isolate', mbw.files)
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
          "}}\n"
      ),
    }
    mbw = self.fake_mbw(files=files)

    def fake_call(cmd, env=None, capture_output=True, input=''):
      del cmd
      del env
      del capture_output
      del input
      mbw.files['/fake_src/out/Default/cc_perftests.runtime_deps'] = (
          'cc_perftests\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl', '//out/Default'
    ],
               mbw=mbw,
               ret=0)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolate', mbw.files)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolated.gen.json',
                  mbw.files)

  def test_multiple_isolate_maps(self):
    files = {
        '/tmp/swarming_targets':
        'cc_perftests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'cc_perftests': {"
         "  'label': '//cc:cc_perftests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/testing/buildbot/gn_isolate_map2.pyl':
        ("{'cc_perftests2': {"
         "  'label': '//cc:cc_perftests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }
    mbw = self.fake_mbw(files=files)

    def fake_call(cmd, env=None, capture_output=True, input=''):
      del cmd
      del env
      del capture_output
      del input
      mbw.files['/fake_src/out/Default/cc_perftests.runtime_deps'] = (
          'cc_perftests_fuzzer\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map2.pyl', '//out/Default'
    ],
               mbw=mbw,
               ret=0)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolate', mbw.files)
    self.assertIn('/fake_src/out/Default/cc_perftests.isolated.gen.json',
                  mbw.files)


  def test_duplicate_isolate_maps(self):
    files = {
        '/tmp/swarming_targets':
        'cc_perftests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'cc_perftests': {"
         "  'label': '//cc:cc_perftests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/testing/buildbot/gn_isolate_map2.pyl':
        ("{'cc_perftests': {"
         "  'label': '//cc:cc_perftests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        'c:\\fake_src\out\Default\cc_perftests.exe.runtime_deps':
        ("cc_perftests\n"),
    }
    mbw = self.fake_mbw(files=files, win32=True)
    # Check that passing duplicate targets into mb fails.
    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map2.pyl', '//out/Default'
    ],
               mbw=mbw,
               ret=1)


  def test_isolate(self):
    files = {
        '/fake_src/out/Default/toolchain.ninja':
        "",
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/base_unittests.runtime_deps':
        ("base_unittests\n"),
    }
    self.check([
        'isolate', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'
    ],
               files=files,
               ret=0)

    # test running isolate on an existing build_dir
    files['/fake_src/out/Default/args.gn'] = 'is_debug = true\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

    # Existing build dir that uses a .gni import.
    files['/fake_src/out/Default/args.gn'] = 'import("//import/args.gni")\n'
    files['/fake_src/import/args.gni'] = 'is_debug = true\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files,
               ret=0)

  def test_dedup_runtime_deps(self):
    files = {
        '/tmp/swarming_targets':
        'base_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }

    mbw = self.fake_mbw(files)

    def fake_call(cmd, env=None, capture_output=True, input=''):
      del cmd
      del env
      del capture_output
      del input
      mbw.files['/fake_src/out/Default/base_unittests.runtime_deps'] = (
          'base_unittests\n'
          '../../filters/some_filter/\n'
          '../../filters/some_filter/foo\n'
          '../../filters/another_filter/hoo\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '//out/Default'
    ],
               mbw=mbw,
               ret=0)
    self.assertIn('/fake_src/out/Default/base_unittests.isolate', mbw.files)
    files = mbw.files.get('/fake_src/out/Default/base_unittests.isolate')
    self.assertIn('../../filters/some_filter', files)
    self.assertNotIn('../../filters/some_filter/foo', files)
    self.assertIn('../../filters/another_filter/hoo', files)

  def test_gen_isolate_generated_dir(self):
    files = {
        '/tmp/swarming_targets':
        'base_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }

    mbw = self.fake_mbw(files)

    def fake_call(cmd, env=None, capture_output=True, input=''):
      del cmd
      del env
      del capture_output
      del input
      mbw.files['/fake_src/out/Default/base_unittests.runtime_deps'] = (
          'test_data/\n')
      return 0, '', ''

    mbw.Call = fake_call

    self.check([
        'gen', '-c', 'debug_remoteexec', '--swarming-targets-file',
        '/tmp/swarming_targets', '//out/Default'
    ],
               mbw=mbw,
               ret=1)
    files = mbw.files.get('/fake_src/out/Default/base_unittests.isolate')

    expected_err = ('error: gn `data` items may not list generated directories;'
                    ' list files in directory instead for:\n'
                    '//out/Default/test_data/\n')
    self.assertIn(expected_err, mbw.out)

  def test_isolate_dir(self):
    files = {
        '/fake_src/out/Default/toolchain.ninja':
        "",
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }
    mbw = self.fake_mbw(files=files)
    mbw.cmds.append((0, '', ''))  # Result of `gn gen`
    mbw.cmds.append((0, '', ''))  # Result of `autoninja`

    # Result of `gn desc runtime_deps`
    mbw.cmds.append((0, 'base_unitests\n../../test_data/\n', ''))
    self.check([
        'isolate', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=0,
               err='')

  def test_isolate_generated_dir(self):
    files = {
        '/fake_src/out/Default/toolchain.ninja':
        "",
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
    }
    mbw = self.fake_mbw(files=files)
    mbw.cmds.append((0, '', ''))  # Result of `gn gen`
    mbw.cmds.append((0, '', ''))  # Result of `autoninja`

    # Result of `gn desc runtime_deps`
    mbw.cmds.append((0, 'base_unitests\ntest_data/\n', ''))
    expected_err = ('error: gn `data` items may not list generated directories;'
                    ' list files in directory instead for:\n'
                    '//out/Default/test_data/\n')
    self.check([
        'isolate', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=1)
    self.assertEqual(mbw.out[-len(expected_err):], expected_err)


  def test_run(self):
    files = {
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/base_unittests.runtime_deps':
        ("base_unittests\n"),
    }
    mbw = self.check(
        ['run', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'],
        files=files,
        ret=0)
    # pylint: disable=line-too-long
    self.assertEqual(
        mbw.files['/fake_src/out/Default/base_unittests.isolate'],
          '{"variables": {"command": ["vpython3", "../../testing/test_env.py", "./base_unittests", "--test-launcher-bot-mode", "--asan=0", "--lsan=0", "--msan=0", "--tsan=0", "--cfi-diag=0"], "files": ["../../.vpython3", "../../testing/test_env.py"]}}\n')
    # pylint: enable=line-too-long

  def test_run_swarmed(self):
    files = {
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/base_unittests.runtime_deps':
        ("base_unittests\n"),
        '/fake_src/out/Default/base_unittests.archive.json':
        ("{\"base_unittests\":\"fake_hash\"}"),
        '/fake_src/third_party/depot_tools/cipd_manifest.txt':
        ("# vpython\n"
         "/some/vpython/pkg  git_revision:deadbeef\n"),
    }

    task_json = json.dumps({'tasks': [{'task_id': '00000'}]})
    collect_json = json.dumps({'00000': {'results': {}}})

    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    original_impl = mbw.ToSrcRelPath

    def to_src_rel_path_stub(path):
      if path.endswith('base_unittests.archive.json'):
        return 'base_unittests.archive.json'
      return original_impl(path)

    mbw.ToSrcRelPath = to_src_rel_path_stub

    self.check([
        'run', '-s', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=0)

    # Specify a custom dimension via '-d'.
    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    mbw.ToSrcRelPath = to_src_rel_path_stub
    self.check([
        'run', '-s', '-c', 'debug_remoteexec', '-d', 'os', 'Win7',
        '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=0)

    # Use the internal swarming server via '--internal'.
    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    mbw.ToSrcRelPath = to_src_rel_path_stub
    self.check([
        'run', '-s', '--internal', '-c', 'debug_remoteexec', '//out/Default',
        'base_unittests'
    ],
               mbw=mbw,
               ret=0)

  def test_run_swarmed_task_failure(self):
    files = {
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/base_unittests.runtime_deps':
        ("base_unittests\n"),
        '/fake_src/out/Default/base_unittests.archive.json':
        ("{\"base_unittests\":\"fake_hash\"}"),
        '/fake_src/third_party/depot_tools/cipd_manifest.txt':
        ("# vpython\n"
         "/some/vpython/pkg  git_revision:deadbeef\n"),
    }

    task_json = json.dumps({'tasks': [{'task_id': '00000'}]})
    collect_json = json.dumps({'00000': {'results': {'exit_code': 1}}})

    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    original_impl = mbw.ToSrcRelPath

    def to_src_rel_path_stub(path):
      if path.endswith('base_unittests.archive.json'):
        return 'base_unittests.archive.json'
      return original_impl(path)

    mbw.ToSrcRelPath = to_src_rel_path_stub

    self.check([
        'run', '-s', '-c', 'debug_remoteexec', '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=1)
    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    mbw.ToSrcRelPath = to_src_rel_path_stub
    self.check([
        'run', '-s', '-c', 'debug_remoteexec', '-d', 'os', 'Win7',
        '//out/Default', 'base_unittests'
    ],
               mbw=mbw,
               ret=1)

  def test_lookup(self):
    self.check(['lookup', '-c', 'debug_remoteexec'],
               ret=0,
               out=('\n'
                    'Writing """\\\n'
                    'is_debug = true\n'
                    'use_remoteexec = true\n'
                    '""" to _path_/args.gn.\n\n'
                    '/fake_src/buildtools/linux64/gn gen _path_\n'))

  def gen_starlark_gn_args_mbw(self, gn_args_json):
    files = {
        self.path('/fake_src/tools/mb/mb_config.pyl'):
        CONFIG_STARLARK_GN_ARGS,
        self.path('/fake_src/tools/mb/../../infra/config/generated/builders/'
                  'gn_args_locations.json'):
        TEST_GN_ARGS_LOCATIONS_JSON,
        self.path('/fake_src/tools/mb/../../infra/config/generated/builders/'
                  'ci/linux-official/gn-args.json'):
        gn_args_json,
    }
    return self.fake_mbw(files=files, win32=is_win())

  def test_lookup_starlark_gn_args(self):
    mbw = self.gen_starlark_gn_args_mbw(TEST_GN_ARGS_JSON)
    expected_out = ('\n'
                    'Writing """\\\n'
                    'bool_arg_lower_case = true\n'
                    'dict_arg = { bool = true\n'
                    'list = [ "foo", "bar", "baz" ]\n'
                    'string = "foo" }\n'
                    'string_arg = "has double quotes"\n'
                    'string_list_arg = [ "foo", "bar", "baz" ]\n'
                    '""" to _path_/args.gn.\n\n')
    if sys.platform == 'win32':
      expected_out += 'c:\\fake_src\\buildtools\\win\\gn.exe gen _path_\n'
    else:
      expected_out += '/fake_src/buildtools/linux64/gn gen _path_\n'
    self.check(['lookup', '-m', 'chromium', '-b', 'linux-official'],
               mbw=mbw,
               ret=0,
               out=expected_out)

  def test_lookup_starlark_gn_args_specified_phase(self):
    mbw = self.gen_starlark_gn_args_mbw(TEST_GN_ARGS_JSON)
    self.check([
        'lookup', '-m', 'chromium', '-b', 'linux-official', '--phase', 'phase_1'
    ],
               mbw=mbw,
               ret=1)
    self.assertIn(
        'MBErr: Must not specify a build --phase '
        'for linux-official on chromium', mbw.out)

  def test_lookup_starlark_phased_gn_args(self):
    mbw = self.gen_starlark_gn_args_mbw(TEST_PHASED_GN_ARGS_JSON)
    expected_out = ('\n'
                    'Writing """\\\n'
                    'bool_arg_lower_case = false\n'
                    'string_arg = "second phase"\n'
                    '""" to _path_/args.gn.\n\n')
    if sys.platform == 'win32':
      expected_out += 'c:\\fake_src\\buildtools\\win\\gn.exe gen _path_\n'
    else:
      expected_out += '/fake_src/buildtools/linux64/gn gen _path_\n'
    self.check([
        'lookup', '-m', 'chromium', '-b', 'linux-official', '--phase', 'phase_2'
    ],
               mbw=mbw,
               ret=0,
               out=expected_out)

  def test_lookup_starlark_phased_gn_args_no_phase(self):
    mbw = self.gen_starlark_gn_args_mbw(TEST_PHASED_GN_ARGS_JSON)
    self.check(['lookup', '-m', 'chromium', '-b', 'linux-official'],
               mbw=mbw,
               ret=1)
    self.assertIn(
        'MBErr: Must specify a build --phase for linux-official on chromium',
        mbw.out)

  def test_lookup_starlark_phased_gn_args_wrong_phase(self):
    mbw = self.gen_starlark_gn_args_mbw(TEST_PHASED_GN_ARGS_JSON)
    self.check([
        'lookup', '-m', 'chromium', '-b', 'linux-official', '--phase', 'phase_3'
    ],
               mbw=mbw,
               ret=1)
    self.assertIn(
        'MBErr: Phase phase_3 doesn\'t exist for linux-official on chromium',
        mbw.out)

  def test_lookup_gn_args_with_non_existent_gn_args_location_file(self):
    files = {
        self.path('/fake_src/tools/mb/mb_config.pyl'):
        textwrap.dedent("""\
            {
              'gn_args_locations_files': [
                '../../infra/config/generated/builders/gn_args_locations.json',
              ],
              'builder_groups': {
                'fake-group': {
                  'fake-builder': 'fake-config',
                },
              },
              'configs': {
                'fake-config': [],
              },
              'mixins': {},
            }
        """)
    }
    mbw = self.fake_mbw(files=files, win32=is_win())
    self.check(['lookup', '-m', 'fake-group', '-b', 'fake-builder'],
               mbw=mbw,
               ret=0)

  def test_quiet_lookup(self):
    self.check(['lookup', '-c', 'debug_remoteexec', '--quiet'],
               ret=0,
               out=('is_debug = true\n'
                    'use_remoteexec = true\n'))

  def test_help(self):
    orig_stdout = sys.stdout
    try:
      sys.stdout = io.StringIO()
      self.assertRaises(SystemExit, self.check, ['-h'])
      self.assertRaises(SystemExit, self.check, ['help'])
      self.assertRaises(SystemExit, self.check, ['help', 'gen'])
    finally:
      sys.stdout = orig_stdout

  def test_multiple_phases(self):
    # Check that not passing a --phase to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_builder_group', '-b',
                      'fake_multi_phase'], ret=1)
    self.assertIn('Must specify a build --phase', mbw.out)

    # Check that passing a --phase to a single-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_builder_group', '-b',
                      'fake_builder', '--phase', 'phase_1'], ret=1)
    self.assertIn('Must not specify a build --phase', mbw.out)

    # Check that passing a wrong phase key to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_builder_group', '-b',
                      'fake_multi_phase', '--phase', 'wrong_phase'], ret=1)
    self.assertIn('Phase wrong_phase doesn\'t exist', mbw.out)

    # Check that passing a correct phase key to a multi-phase builder passes.
    mbw = self.check(['lookup', '-m', 'fake_builder_group', '-b',
                      'fake_multi_phase', '--phase', 'phase_1'], ret=0)
    self.assertIn('phase = 1', mbw.out)

    mbw = self.check(['lookup', '-m', 'fake_builder_group', '-b',
                      'fake_multi_phase', '--phase', 'phase_2'], ret=0)
    self.assertIn('phase = 2', mbw.out)

  def test_recursive_lookup(self):
    files = {
        '/fake_src/build/args/fake.gn': (
          'enable_doom_melon = true\n'
          'enable_antidoom_banana = true\n'
        )
    }
    self.check([
        'lookup', '-m', 'fake_builder_group', '-b', 'fake_args_file',
        '--recursive'
    ],
               files=files,
               ret=0,
               out=('dcheck_always_on = false\n'
                    'is_debug = false\n'
                    'use_remoteexec = true\n'))

  def test_train(self):
    mbw = self.fake_mbw()
    temp_dir = mbw.TempDir()
    self.check(['train', '--expectations-dir', temp_dir], mbw=mbw, ret=0)
    self.assertIn(os.path.join(temp_dir, 'fake_builder_group.json'), mbw.files)

  def test_validate(self):
    mbw = self.fake_mbw()
    self.check(['validate'], mbw=mbw, ret=0)

  def test_bad_validate(self):
    mbw = self.fake_mbw()
    mbw.files[mbw.default_config] = TEST_BAD_CONFIG
    self.check(['validate', '-f', mbw.default_config], mbw=mbw, ret=1)

  def test_duplicate_validate(self):
    mbw = self.fake_mbw()
    mbw.files[mbw.default_config] = TEST_DUP_CONFIG
    self.check(['validate'], mbw=mbw, ret=1)
    self.assertIn(
        'Duplicate configs detected. When evaluated fully, the '
        'following configs are all equivalent: \'some_config\', '
        '\'some_other_config\'.', mbw.out)

  def test_good_expectations_validate(self):
    mbw = self.fake_mbw()
    # Train the expectations normally.
    temp_dir = mbw.TempDir()
    self.check(['train', '--expectations-dir', temp_dir], mbw=mbw, ret=0)
    # Immediately validating them should pass.
    self.check(['validate', '--expectations-dir', temp_dir], mbw=mbw, ret=0)

  def test_bad_expectations_validate(self):
    mbw = self.fake_mbw()
    # Train the expectations normally.
    temp_dir = mbw.TempDir()
    self.check(['train', '--expectations-dir', temp_dir], mbw=mbw, ret=0)
    # Remove one of the expectation files.
    mbw.files.pop(os.path.join(temp_dir, 'fake_builder_group.json'))
    # Now validating should fail.
    self.check(['validate', '--expectations-dir', temp_dir], mbw=mbw, ret=1)
    self.assertIn('Expectations out of date', mbw.out)

  def test_build_command_unix(self):
    files = {
        '/fake_src/out/Default/toolchain.ninja':
        '',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ('{"base_unittests": {'
         '  "label": "//base:base_unittests",'
         '  "type": "console_test_launcher",'
         '  "args": [],'
         '}}\n')
    }

    mbw = self.fake_mbw(files)
    self.check(['run', '//out/Default', 'base_unittests'], mbw=mbw, ret=0)
    self.assertIn(['autoninja', '-C', 'out/Default', 'base_unittests'],
                  mbw.calls)

  def test_build_command_windows(self):
    files = {
        'c:\\fake_src\\out\\Default\\toolchain.ninja':
        '',
        'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl':
        ('{"base_unittests": {'
         '  "label": "//base:base_unittests",'
         '  "type": "console_test_launcher",'
         '  "args": [],'
         '}}\n')
    }

    mbw = self.fake_mbw(files, True)
    self.check(['run', '//out/Default', 'base_unittests'], mbw=mbw, ret=0)
    self.assertIn(['autoninja.bat', '-C', 'out\\Default', 'base_unittests'],
                  mbw.calls)

  def test_lookup_non_existent_builder_group(self):
    """Ensure correct behavior when non-existent builder group is specified.

    Lookups for builders that don't exist in the config file return a different
    exit code so that they can be distinguished from other errors.
    """
    mbw = self.fake_mbw()
    self.check(
        [
            'lookup', '-m', 'non-existent-builder-group', '-b',
            'non-existent-builder'
        ],
        mbw=mbw,
        ret=2,
    )
    self.assertIn(
        'MBErr: Builder group name "non-existent-builder-group" not found',
        mbw.out)

  def test_lookup_non_existent_builder(self):
    """Ensure correct behavior when non-existent builder is specified.

    Lookups for builders that don't exist in the config file return a different
    exit code so that they can be distinguished from other errors.
    """
    mbw = self.fake_mbw()
    self.check(
        ['lookup', '-m', 'fake_builder_group', '-b', 'non-existent-builder'],
        mbw=mbw,
        ret=2)
    self.assertIn(
        'MBErr: Builder name "non-existent-builder" not found under groups',
        mbw.out)


if __name__ == '__main__':
  unittest.main()
