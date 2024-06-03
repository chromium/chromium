#! /usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Linker wrapper that performs distributed ThinLTO on Reclient.
#
# Usage: Pass the original link command as parameters to this script.
# E.g. original: lld-link -out:foo foo.obj
# Becomes: remote_link.py lld-link -out:foo foo.obj

import argparse
import errno
import io
import os
import re
import shlex
import subprocess
import sys
from collections import namedtuple
from pipes import quote as shquote
from tempfile import NamedTemporaryFile

# Type returned by analyze_args.
AnalyzeArgsResult = namedtuple('AnalyzeArgsResult', [
    'output', 'linker', 'compiler', 'splitfile', 'index_inputs', 'index_params',
    'codegen', 'codegen_params', 'final_inputs', 'final_params'
])


def autoninja():
  """
  Returns the name of the autoninja executable to invoke.
  """
  name = os.path.normpath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', 'third_party',
                   'depot_tools', 'autoninja'))
  if os.name == 'nt':
    return name + '.bat'
  else:
    return name


def ensure_dir(path):
  """
  Creates path as a directory if it does not already exist.
  """
  if not path:
    return
  try:
    os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def ensure_file(path):
  """
  Creates an empty file at path if it does not already exist.
  Also creates directories as needed.
  """
  ensure_dir(os.path.dirname(path))
  try:
    fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o644)
    os.close(fd)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise


def exe_suffix():
  if os.name == 'nt':
    return '.exe'
  else:
    return ''


def is_bitcode_file(path):
  """
  Returns True if path contains a LLVM bitcode file, False if not.
  """
  with open(path, 'rb') as f:
    return f.read(4) == b'BC\xc0\xde'


def is_thin_archive(path):
  """
  Returns True if path refers to a thin archive (ar file), False if not.
  """
  with open(path, 'rb') as f:
    return f.read(8) == b'!<thin>\n'


def names_in_archive(path, ar_path):
  """
  Yields the member names in the archive file at path.
  """
  proc = subprocess.run([ar_path, "t", path], stdout=subprocess.PIPE)
  for line in proc.stdout.splitlines():
    # Using UTF-8 here gives us a fighting chance if someone decides to use
    # non-US-ASCII characters in a file name, and backslashreplace gives us
    # a human-readable representation of any undecodable bytes we might
    # encounter.
    yield line.decode('UTF-8', 'backslashreplace').rstrip()


def ninjaenc(s):
  """
  Encodes string s for use in ninja files.
  """
  return s.replace('$', '$$')


def ninjajoin(l):
  """
  Encodes list of strings l to a string encoded for use in a ninja file.
  """
  return ' '.join(map(ninjaenc, l))


def parse_args(args):
  """
  Parses the command line and returns a structure with the results.
  """
  # The basic invocation is to pass in the command line that would be used
  # for a local ThinLTO link. Optionally, this may be preceded by options
  # that set some values for this script. If these optional options are
  # present, they must be followed by '--'.
  ap = argparse.ArgumentParser()
  ap.add_argument('--generate',
                  action='store_true',
                  help='generate ninja file, but do not invoke it.')
  ap.add_argument('--wrapper', help='path to remote exec wrapper.')
  ap.add_argument('--jobs', '-j', help='maximum number of concurrent jobs.')
  ap.add_argument('--no-wrapper',
                  action='store_true',
                  help='do not use remote exec wrapper.')
  ap.add_argument('--allowlist',
                  action='store_true',
                  help='act as if the target is on the allow list.')
  ap.add_argument('--ar-path', help='path to ar or llvm-ar.', required=True)
  try:
    splitpos = args.index('--')
  except:
    raise Exception("Must separate linker args from wrapper args using --")
  parsed = ap.parse_args(args[1:splitpos])
  rest = args[(splitpos + 1):]
  parsed.linker = rest[0]
  parsed.linker_args = rest[1:]
  return parsed


def report_run(cmd, *args, **kwargs):
  """
  Runs a command using subprocess.check_call, first writing the command line
  to standard error.
  """
  sys.stderr.write('%s: %s\n' % (sys.argv[0], ' '.join(map(shquote, cmd))))
  sys.stderr.flush()
  return subprocess.check_call(cmd, *args, **kwargs)


class RemoteLinkBase(object):
  """
  Base class used by RemoteLinkUnix and RemoteLinkWindows.
  """
  # Defaults.
  wrapper = 'rewrapper'
  jobs = None

  # These constants should work across platforms.
  DATA_SECTIONS_RE = re.compile('-f(no-)?data-sections|[-/]Gw(-)?',
                                re.IGNORECASE)
  FUNCTION_SECTIONS_RE = re.compile('-f(no-)?function-sections|[-/]Gy(-)?',
                                    re.IGNORECASE)
  LIB_RE = re.compile('.*\\.(?:a|r?lib)', re.IGNORECASE)
  # LTO_RE matches flags we want to pass in the thin link step but not in the
  # native link step.
  # Continue to pass -flto and -fsanitize flags in the native link even though
  # they're not normally necessary because clang needs them to build with CFI.
  LTO_RE = re.compile('|'.join((
      '-Wl,-plugin-opt=.*',
      '-Wl,--lto.*',
      '-Wl,--thin.*',
  )))
  MLLVM_RE = re.compile('(?:-Wl,)?([-/]mllvm)[:=,]?(.*)', re.IGNORECASE)
  OBJ_RE = re.compile('(.*)\\.(o(?:bj)?)', re.IGNORECASE)

  def _no_codegen(self, args):
    """
    Helper function for the case where no distributed code generation
    is necessary. It invokes the original command, unless --generate
    was passed, in which case it informs the user that no code generation
    is necessary.
    """
    if args.generate:
      sys.stderr.write(
          'No code generation required; no ninja file generated.\n')
      return 5  # Indicates no code generation required.
    return subprocess.call([args.linker] + args.linker_args)

  def transform_codegen_param(self, param):
    return self.transform_codegen_param_common(param)

  def transform_codegen_param_common(self, param):
    """
    If param is a parameter relevant to code generation, returns the
    parameter in a form that is suitable to pass to clang.  For values
    of param that are not relevant to code generation, returns None.
    """
    match = self.MACHINE_RE.match(param)
    if match and match.group(1).lower() in ['x86', 'i386', 'arm', '32']:
      return ['-m32']
    match = self.MLLVM_RE.match(param)
    if match:
      if match.group(2):
        return ['-mllvm', match.group(2)]
      else:
        return ['-mllvm']
    if (param.startswith('-f') and not param.startswith('-flto')
        and not param.startswith('-fsanitize')
        and not param.startswith('-fthinlto')
        and not param.startswith('-fwhole-program')):
      return [param]
    if param.startswith('-g'):
      return [param]
    if param.startswith('-m'):
      # Note: -mllvm is handled separately above.
      return [param]
    if param.startswith('--target'):
      return [param]
    return None

  def output_path(self, args):
    """
    Analyzes command line arguments in args and returns the output
    path if one is specified by args. If no output path is specified
    by args, returns None.
    """
    i = 2
    while i < len(args):
      output, next_i = self.process_output_param(args, i)
      if output is not None:
        return output
      i = next_i
    return None

  def write_rsp(self, path, params):
    """
    Writes params to a newly created response file at path.
    """
    ensure_dir(os.path.basename(path))
    with open(path, 'wb') as f:
      f.write('\n'.join(map(self.rspenc, params)).encode('UTF-8'))

  def rspenc(self, param):
    """
    Encodes param for use in an rsp file.
    """
    return param.replace('\\%', '%')

  def expand_rsp(self, rspname):
    """
    Returns the parameters found in the response file at rspname.
    """
    with open(rspname) as f:
      return shlex.split(f.read())

  def expand_args_rsps(self, args):
    """
    Yields args, expanding @rsp file references into the commands mentioned
    in the rsp file.
    """
    result = []
    for arg in args:
      if len(arg) > 0 and arg[0] == '@':
        for x in self.expand_rsp(arg[1:]):
          yield x
      else:
        yield arg

  def expand_archives(self, args, ar_path):
    """
    Yields the parameters in args, with archives replaced by a sequence
    of '--start-lib', the member names, and '--end-lib'. This is used to get a
    command line where members of archives are mentioned explicitly, but we
    still get the same semantics as using archive files, namely that the object
    files are only linked in if they provided needed symbol definitions.
    Most of the archives encountered in the Chromium link process are
    "thin archives", which means they're just directories of files that are
    already on disk - and all we need to do is enumerate those files instead
    of actually extracting anything. Occasionally we encounter a real archive
    and its contents need to be extracted.
    """
    for arg in args:
      if arg.startswith("./"):
        arg = arg[2:]
      if self.LIB_RE.match(arg) and os.path.exists(arg):
        yield (self.WL + '--start-lib')
        if is_thin_archive(arg):
          for name in names_in_archive(arg, ar_path):
            yield (name)
        else:
          arg_encoded = arg.replace("..", "parent_dir")
          extractdir = os.path.join("expanded_archives", arg_encoded)
          if not os.path.exists(extractdir):
            os.makedirs(extractdir)
          subprocess.run([ar_path, "--output", extractdir, "x", arg])
          for name in names_in_archive(arg, ar_path):
            yield (os.path.join(extractdir, name))
        yield (self.WL + '--end-lib')
      else:
        yield (arg)

  def analyze_args(self, args, gen_dir, common_dir, use_common_objects,
                   ar_path):
    """
    Analyzes the command line arguments in args.
    If no ThinLTO code generation is necessary, returns None.
    Else, returns an AnalyzeArgsResult value.

    Args:
      args: the command line as returned by parse_args().
      gen_dir: directory in which to generate files specific to this target.
      common_dir: directory for file shared among targets.
      use_common_objects: if True, native object files are shared with
        other targets.
    """
    # If we're invoking the NaCl toolchain, don't do distributed code
    # generation.
    if os.path.basename(args.linker).startswith('pnacl-'):
      return None

    rsp_expanded = list(self.expand_args_rsps(args.linker_args))
    expanded_args = list(self.expand_archives(rsp_expanded, ar_path))

    return self.analyze_expanded_args(expanded_args, args.output, args.linker,
                                      gen_dir, common_dir, use_common_objects)

  def analyze_expanded_args(self, args, output, linker, gen_dir, common_dir,
                            use_common_objects):
    """
    Helper function for analyze_args. This is called by analyze_args after
    expanding rsp files and determining which files are bitcode files, and
    produces codegen_params, final_params, and index_params.

    This function interacts with the filesystem through os.path.exists,
    is_bitcode_file, and ensure_file.
    """
    if 'clang' in os.path.basename(linker):
      compiler = linker
    else:
      compiler_dir = os.path.dirname(linker)
      if compiler_dir:
        compiler_dir += '/'
      else:
        compiler_dir = ''
      compiler = compiler_dir + 'clang-cl' + exe_suffix()

    if use_common_objects:
      obj_dir = common_dir
    else:
      obj_dir = gen_dir

    common_index = common_dir + '/empty.thinlto.bc'
    index_inputs = set()
    index_params = []
    codegen = []
    codegen_params = [
        '-Wno-unused-command-line-argument',
        '-Wno-override-module',
    ]
    final_inputs = set()
    final_params = []
    in_mllvm = [False]

    # Defaults that match those for local linking.
    optlevel = [2]
    data_sections = [True]
    function_sections = [True]

    def extract_opt_level(param):
      """
      If param is a parameter that specifies the LTO optimization level,
      returns the level. If not, returns None.
      """
      match = re.match('(?:-Wl,)?--lto-O(.+)', param)
      if match:
        return match.group(1)
      match = re.match('[-/]opt:.*lldlto=([^:]*)', param, re.IGNORECASE)
      if match:
        return match.group(1)
      return None

    def process_param(param):
      """
      Common code for processing a single parameter from the either the
      command line or an rsp file.
      """

      def helper():
        """
        This exists so that we can use return instead of
        nested if statements to use the first matching case.
        """
        # After -mllvm, just pass on the param.
        if in_mllvm[0]:
          if param.startswith('-Wl,'):
            codegen_params.append(param[4:])
          else:
            codegen_params.append(param)
          in_mllvm[0] = False
          return

        # Check for params that specify LTO optimization level.
        o = extract_opt_level(param)
        if o is not None:
          optlevel[0] = o
          return

        # Check for params that affect code generation.
        cg_param = self.transform_codegen_param(param)
        if cg_param:
          codegen_params.extend(cg_param)
          # No return here, we still want to check for -mllvm.

        # Check for -mllvm.
        match = self.MLLVM_RE.match(param)
        if match and not match.group(2):
          # Next parameter will be the thing to pass to LLVM.
          in_mllvm[0] = True

      # Parameters that override defaults disable the defaults; the
      # final value is set by passing through the parameter.
      if self.DATA_SECTIONS_RE.match(param):
        data_sections[0] = False
      if self.FUNCTION_SECTIONS_RE.match(param):
        function_sections[0] = False

      helper()
      if self.GROUP_RE.match(param):
        return
      index_params.append(param)
      if os.path.exists(param):
        index_inputs.add(param)
        match = self.OBJ_RE.match(param)
        if match and is_bitcode_file(param):
          native = obj_dir + '/' + match.group(1) + '.' + match.group(2)
          if use_common_objects:
            index = common_index
          else:
            index = obj_dir + '/' + param + '.thinlto.bc'
            ensure_file(index)
          codegen.append((os.path.normpath(native), param, index))
        else:
          final_inputs.add(param)
          final_params.append(param)
      elif not self.LTO_RE.match(param):
        final_params.append(param)

    index_params.append(self.WL + self.PREFIX_REPLACE + ';' + obj_dir + '/')
    i = 0
    while i < len(args):
      x = args[i]
      if not self.GROUP_RE.match(x):
        outfile, next_i = self.process_output_param(args, i)
        if outfile is not None:
          index_params.extend(args[i:next_i])
          final_params.extend(args[i:next_i])
          i = next_i - 1
        else:
          process_param(x)
      i += 1

    # If we are not doing ThinLTO codegen, just invoke the original command.
    if len(codegen) < 1:
      return None

    codegen_params.append('-O' + str(optlevel[0]))
    if data_sections[0]:
      codegen_params.append(self.DATA_SECTIONS)
    if function_sections[0]:
      codegen_params.append(self.FUNCTION_SECTIONS)

    if use_common_objects:
      splitfile = None
      for tup in codegen:
        final_params.append(tup[0])
      index_inputs = []
    else:
      splitfile = gen_dir + '/' + output + '.split' + self.OBJ_SUFFIX
      final_params.append(splitfile)
      index_params.append(self.WL + self.OBJ_PATH + splitfile)
      used_obj_file = gen_dir + '/' + os.path.basename(output) + '.objs'
      final_params.append('@' + used_obj_file)

    return AnalyzeArgsResult(
        output=output,
        linker=linker,
        compiler=compiler,
        splitfile=splitfile,
        index_inputs=index_inputs,
        index_params=index_params,
        codegen=codegen,
        codegen_params=codegen_params,
        final_inputs=final_inputs,
        final_params=final_params,
    )

  def gen_ninja(self, ninjaname, params, gen_dir):
    """
    Generates a ninja build file at path ninjaname, using original command line
    params and with objs being a list of bitcode files for which to generate
    native code.
    """
    if self.wrapper:
      wrapper_prefix = ninjaenc(self.wrapper) + ' '
    else:
      wrapper_prefix = ''
    base = gen_dir + '/' + os.path.basename(params.output)
    ensure_dir(gen_dir)
    ensure_dir(os.path.dirname(ninjaname))
    codegen_cmd = ('%s%s -c %s -fthinlto-index=$index %s$bitcode -o $native' %
                   (wrapper_prefix, ninjaenc(params.compiler),
                    ninjajoin(params.codegen_params), self.XIR))
    if params.index_inputs:
      used_obj_file = base + '.objs'
      index_rsp = base + '.index.rsp'
      ensure_dir(os.path.dirname(used_obj_file))
      if params.splitfile:
        ensure_dir(os.path.dirname(params.splitfile))
        # We use grep here to only codegen native objects which are actually
        # used by the native link step. Ninja 1.10 introduced a dyndep feature
        # which allows for a more elegant implementation, but Chromium still
        # uses an older ninja version which doesn't have this feature.
        codegen_cmd = '( ! grep -qF $native %s || %s)' % (
            ninjaenc(used_obj_file), codegen_cmd)

    with open(ninjaname, 'w') as f:
      if params.index_inputs:
        self.write_rsp(index_rsp, params.index_params)
        f.write('\nrule index\n  command = %s %s %s @$rspfile\n' %
                (ninjaenc(params.linker),
                 ninjaenc(self.WL + self.TLTO + '-index-only' + self.SEP) +
                 '$out', self.WL + self.TLTO + '-emit-imports-files'))

      f.write(('\nrule native-link\n  command = %s @$rspname'
               '\n  rspfile = $rspname\n  rspfile_content = $params\n') %
              (ninjaenc(params.linker), ))

      f.write('\nrule codegen\n  command = %s && touch $out\n' %
              (codegen_cmd, ))

      native_link_deps = []
      if params.index_inputs:
        f.write(
            ('\nbuild %s | %s : index %s\n'
             '  rspfile = %s\n'
             '  rspfile_content = %s\n') %
            (ninjaenc(used_obj_file), ninjajoin(
                [x[2] for x in params.codegen]), ninjajoin(params.index_inputs),
             ninjaenc(index_rsp), ninjajoin(params.index_params)))
        native_link_deps.append(used_obj_file)

      for tup in params.codegen:
        obj, bitcode, index = tup
        stamp = obj + '.stamp'
        native_link_deps.append(obj)
        f.write(
            ('\nbuild %s : codegen %s %s\n'
             '  bitcode = %s\n'
             '  index = %s\n'
             '  native = %s\n'
             '\nbuild %s : phony %s\n') % tuple(
                 map(ninjaenc,
                     (stamp, bitcode, index, bitcode, index, obj, obj, stamp))))

      f.write(('\nbuild %s : native-link %s\n'
               '  rspname = %s\n  params = %s\n') %
              (ninjaenc(params.output),
               ninjajoin(list(params.final_inputs) + native_link_deps),
               ninjaenc(base + '.final.rsp'), ninjajoin(params.final_params)))

      f.write('\ndefault %s\n' % (ninjaenc(params.output), ))

  def do_main(self, argv):
    """
    This function contains the main code to run. Not intended to be called
    directly. Call main instead, which returns exit status for failing
    subprocesses.
    """
    args = parse_args(argv)
    args.output = self.output_path(argv[1:])
    if args.output is None:
      return self._no_codegen(args)
    if args.wrapper:
      self.wrapper = args.wrapper
    if args.no_wrapper:
      self.wrapper = None
    if args.jobs:
      self.jobs = int(args.jobs)

    basename = os.path.basename(args.output)
    # Only generate tailored native object files for targets on the allow list.
    # TODO: Find a better way to structure this. There are three different
    # ways we can perform linking: Local ThinLTO, distributed ThinLTO,
    # and distributed ThinLTO with common object files.
    # We expect the distributed ThinLTO variants to be faster, but
    # common object files cannot be used when -fsplit-lto-unit is in effect.
    # Currently, we don't detect this situation. We could, but it might
    # be better to instead move this logic out of this script and into
    # the build system.
    use_common_objects = not (args.allowlist or basename in self.ALLOWLIST)
    common_dir = 'common_objs'
    gen_dir = 'lto.' + basename
    params = self.analyze_args(args, gen_dir, common_dir, use_common_objects,
                               args.ar_path)
    # If we determined that no distributed code generation need be done, just
    # invoke the original command.
    if params is None:
      return self._no_codegen(args)
    if use_common_objects:
      objs = [x[0] for x in params.codegen]
      ensure_file(common_dir + '/empty.thinlto.bc')
    ninjaname = gen_dir + '/build.ninja'
    self.gen_ninja(ninjaname, params, gen_dir)
    if args.generate:
      sys.stderr.write('Generated ninja file %s\n' % (shquote(ninjaname), ))
    else:
      cmd = [autoninja(), '-f', ninjaname]
      if self.jobs:
        cmd.extend(['-j', str(self.jobs)])
      report_run(cmd)
    return 0

  def main(self, argv):
    try:
      return self.do_main(argv)
    except subprocess.CalledProcessError as e:
      return e.returncode


class RemoteLinkWindows(RemoteLinkBase):
  # Target-platform-specific constants.
  WL = ''
  TLTO = '-thinlto'
  SEP = ':'
  DATA_SECTIONS = '-Gw'
  FUNCTION_SECTIONS = '-Gy'
  GROUP_RE = re.compile(WL + '--(?:end|start)-group')
  MACHINE_RE = re.compile('[-/]machine:(.*)', re.IGNORECASE)
  OBJ_PATH = '-lto-obj-path' + SEP
  OBJ_SUFFIX = '.obj'
  OUTPUT_RE = re.compile('[-/]out:(.*)', re.IGNORECASE)
  PREFIX_REPLACE = TLTO + '-prefix-replace' + SEP
  XIR = ''

  ALLOWLIST = {
      'chrome.exe',
      'chrome.dll',
      'chrome_child.dll',
      # TODO: The following targets are on the allow list because the
      # common objects flow does not link them successfully. This should
      # be fixed, after which they can be removed from the list.
      'tls_edit.exe',
  }

  def transform_codegen_param(self, param):
    # In addition to parameters handled by transform_codegen_param_common,
    # we pass on parameters that start in 'G' or 'Q', which are
    # MSVC-style parameters that affect code generation.
    if len(param) >= 2 and param[0] in ['-', '/'] and param[1] in ['G', 'Q']:
      return [param]
    return self.transform_codegen_param_common(param)

  def process_output_param(self, args, i):
    """
    If args[i] is a parameter that specifies the output file,
    returns (output_name, new_i). Else, returns (None, new_i).
    """
    m = self.OUTPUT_RE.match(args[i])
    if m:
      return (os.path.normpath(m.group(1)), i + 1)
    else:
      return (None, i + 1)


if __name__ == '__main__':
  sys.exit(RemoteLinkWindows().main(sys.argv))
