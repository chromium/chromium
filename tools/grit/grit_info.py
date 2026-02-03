#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Tool to determine inputs and outputs of a grit file.
'''


import difflib
import optparse
import os
import posixpath
import sys

from grit import grd_reader
from grit import util

class WrongNumberOfArguments(Exception):
  pass


class GritInfoError(Exception):
  pass


def Outputs(filename,
            defines,
            ids_file,
            target_platform=None,
            translate_genders=False,
            android_output_zip_path=None):
  grd = grd_reader.Parse(filename,
                         defines=defines,
                         tags_to_ignore={'messages'},
                         first_ids_file=ids_file,
                         target_platform=target_platform,
                         translate_genders=translate_genders)

  target = [
      i.GetFilename() for i in grd.GetOutputFiles()
      if i.GetType() != 'android' or android_output_zip_path is None
  ]

  if android_output_zip_path is not None:
    target.append(android_output_zip_path)

  return [t.replace('\\', '/') for t in target]


def GritSourceFiles():
  files = []
  grit_root_dir = os.path.relpath(os.path.dirname(__file__), os.getcwd())
  for root, dirs, filenames in os.walk(grit_root_dir):
    grit_src = [os.path.join(root, f) for f in filenames
                if f.endswith('.py') and not f.endswith('_unittest.py')]
    files.extend(grit_src)
  return sorted(files)


def _GetInputFiles(grd, nodes, filename):
  files = set()
  for node in nodes:
    with node:
      if (node.name == 'structure' or node.name == 'skeleton'
          or (node.name == 'file' and node.parent
              and node.parent.name == 'translations')):
        path = node.GetInputPath()
        if path is not None:
          files.add(grd.ToRealPath(path))

        # If it's a flattened node, grab inlined resources too.
        if node.name == 'structure' and node.attrs.get('flattenhtml') == 'true':
          node.RunPreSubstitutionGatherer()
          files.update(node.GetHtmlResourceFilenames())
      elif node.name == 'grit':
        first_ids_file = node.GetFirstIdsFile()
        if first_ids_file:
          files.add(first_ids_file)
      elif node.name == 'include':
        files.add(grd.ToRealPath(node.GetInputPath()))
        # If it's a flattened node, grab inlined resources too.
        if node.attrs.get('flattenhtml') == 'true':
          files.update(node.GetHtmlResourceFilenames())
      elif node.name == 'part':
        files.add(
            util.normpath(
                os.path.join(os.path.dirname(filename), node.GetInputPath())))
  return files


def Inputs(filename, defines, ids_file, target_platform=None):
  grd = grd_reader.Parse(
      filename, debug=False, defines=defines, tags_to_ignore={'message'},
      first_ids_file=ids_file, target_platform=target_platform)
  files = set()
  for lang, ctx, fallback in grd.GetConfigurations():
    # TODO(tdanderson): Refactor all places which perform the action of setting
    #                   output attributes on the root. See crbug.com/503637.
    grd.SetOutputLanguage(lang or grd.GetSourceLanguage())
    grd.SetOutputContext(ctx)
    grd.SetFallbackToDefaultLayout(fallback)
    files.update(_GetInputFiles(grd, grd.ActiveDescendants(), filename))

  cwd = os.getcwd()
  return [os.path.relpath(f, cwd) for f in sorted(files)]


def AllInputs(filename, defines, ids_file, source_root, target_platform=None):
  grd = grd_reader.Parse(filename,
                         debug=False,
                         defines=defines,
                         tags_to_ignore={'message'},
                         first_ids_file=ids_file,
                         target_platform=target_platform,
                         skip_validation_checks=True)
  files = _GetInputFiles(grd, grd.Preorder(), filename)

  # Inputs are usually absolute or relative to CWD (build dir).
  # Fix them so they are relative to source_root.
  abs_inputs = [os.path.abspath(i) for i in files]
  source_root_abs = os.path.abspath(source_root)
  # Normalize paths to use forward slashes for consistency across platforms,
  # specifically for Windows where os.path.relpath might return backslashes.
  files = [
      os.path.relpath(i, source_root_abs).replace('\\', '/') for i in abs_inputs
  ]
  return sorted(files)


def CheckGritDeps(filename, defines, ids_file, target_platform, deps_file,
                  stamp_file, source_root):
  actual_output = AllInputs(filename, defines, ids_file, source_root,
                            target_platform)

  try:
    with open(deps_file) as f:
      expected_output = [
          l for l in f.read().strip().splitlines() if not l.startswith('#')
      ]
  except FileNotFoundError:
    raise GritInfoError(f"Error: .gritdeps file not found at {deps_file}")

  if actual_output != expected_output:
    msg = f"Error: gritdeps mismatch for {deps_file}\nDiff:\n"
    diff = difflib.unified_diff(expected_output,
                                actual_output,
                                fromfile='expected',
                                tofile='actual',
                                lineterm='')
    msg += '\n'.join(diff)
    raise GritInfoError(msg)

  with open(stamp_file, 'w') as f:
    f.write('Matched\n')

  return ''


def PrintUsage():
  print('USAGE: ./grit_info.py --inputs [-D foo] [-f resource_ids] <grd-file>')
  print(
      '       ./grit_info.py --all-inputs [-D foo] [-f resource_ids] <grd-file>'
  )
  print('       ./grit_info.py --outputs [-D foo] [-f resource_ids] ' +
        '<out-prefix> <grd-file>')
  print('       ./grit_info.py --check-grit-deps <gritdeps-file> ' +
        '--stamp <stamp-file> --source-root <source-root> <grd-file>')
  print('       ./grit_info.py --help')


def DoMain(argv):
  os.environ['cwd'] = os.getcwd()

  parser = optparse.OptionParser()
  parser.add_option("--inputs",
                    action="store_true",
                    dest="inputs",
                    help="Determines the inputs for the given grit file for "
                    "the specific build configuration defined by defines "
                    "and target platform.")
  parser.add_option("--all-inputs",
                    action="store_true",
                    dest="all_inputs",
                    help="Determines all possible inputs for the given grit "
                    "file, regardless of the build configuration.")
  parser.add_option("--outputs", action="store_true", dest="outputs")
  parser.add_option("--check-grit-deps", dest="check_grit_deps")
  parser.add_option("--stamp", dest="stamp_file")
  parser.add_option("--source-root", dest="source_root")
  parser.add_option("-D", action="append", dest="defines", default=[])
  # grit build also supports '-E KEY=VALUE', support that to share command
  # line flags.
  parser.add_option("-E", action="append", dest="build_env", default=[])
  parser.add_option("-p", action="store", dest="predetermined_ids_file")
  parser.add_option("-w", action="append", dest="allowlist_files", default=[])
  parser.add_option("-f", dest="ids_file", default="")
  parser.add_option("-t", dest="target_platform", default=None)
  parser.add_option("--translate-genders",
                    action="store_true",
                    dest="translate_genders")
  parser.add_option("--android-output-zip-path",
                    dest="android_output_zip_path",
                    default=None)

  options, args = parser.parse_args(argv)

  defines = {}
  for define in options.defines:
    name, val = util.ParseDefine(define)
    defines[name] = val

  for env_pair in options.build_env:
    (env_name, env_value) = env_pair.split('=', 1)
    os.environ[env_name] = env_value

  if options.inputs:
    if len(args) > 1:
      raise WrongNumberOfArguments("Expected 0 or 1 arguments for --inputs.")

    inputs = []
    if len(args) == 1:
      filename = args[0]
      inputs = Inputs(filename, defines, options.ids_file,
                      options.target_platform)

    # Add in the grit source files.  If one of these change, we want to re-run
    # grit.
    inputs.extend(GritSourceFiles())
    inputs = [f.replace('\\', '/') for f in inputs]

    if len(args) == 1:
      # Include grd file as second input (works around gyp expecting it).
      inputs.insert(1, args[0])
    if options.allowlist_files:
      inputs.extend(options.allowlist_files)
    return '\n'.join(inputs)
  elif options.all_inputs:
    if len(args) != 1:
      raise WrongNumberOfArguments("Expected 1 argument for --all-inputs")

    filename = args[0]

    # Here we don't add the grit source files since we already specified
    # them in grit_rule.py and eventually we want to switch to
    # action_with_pydeps
    source_root = options.source_root
    if not source_root:
      source_root = os.getcwd()
    inputs = AllInputs(filename, defines, options.ids_file, source_root,
                       options.target_platform)

    output = '# Generated by running:\n'
    output += '#   %s --all-inputs %s > %s.gritdeps\n' % (os.path.relpath(
        sys.argv[0], source_root), filename, filename)
    output += '\n'.join(inputs)
    return output
  elif options.check_grit_deps:
    if len(args) != 1:
      raise WrongNumberOfArguments("Expected 1 argument for --check-grit-deps")
    if not options.stamp_file:
      raise WrongNumberOfArguments(
          "Expected --stamp argument for --check-grit-deps")
    if not options.source_root:
      raise WrongNumberOfArguments(
          "Expected --source-root argument for --check-grit-deps")

    filename = args[0]
    return CheckGritDeps(filename, defines, options.ids_file,
                         options.target_platform, options.check_grit_deps,
                         options.stamp_file, options.source_root)
  elif options.outputs:
    if len(args) != 2:
      raise WrongNumberOfArguments(
          "Expected exactly 2 arguments for --outputs.")

    prefix, filename = args
    outputs = [
        posixpath.join(prefix, f) for f in Outputs(
            filename, defines, options.ids_file, options.target_platform,
            options.translate_genders, options.android_output_zip_path)
    ]
    return '\n'.join(outputs)
  else:
    raise WrongNumberOfArguments(
        "Expected --inputs, --all-inputs, or --outputs.")


def main(argv):
  try:
    result = DoMain(argv[1:])
  except WrongNumberOfArguments as e:
    PrintUsage()
    print(e)
    return 1
  except GritInfoError as e:
    print(e, file=sys.stderr)
    return 1

  if result:
    print(result)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
