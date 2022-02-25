# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import io
import json
import os
import sys

# For Node, EvaluateExpression
import grit.node.base
# For CheckConditionalElements
import grit.format.html_inline

_CWD = os.getcwd()


class PreprocessIfExprNode(grit.node.base.Node):
  def __init__(self):
    super(PreprocessIfExprNode, self).__init__()

  def PreprocessIfExpr(self, content):
    return grit.format.html_inline.CheckConditionalElements(self, content)

  def EvaluateCondition(self, expr):
    return grit.node.base.Node.EvaluateExpression(expr, self.defines,
                                                  self.target_platform, {})

  def SetDefines(self, defines):
    self.defines = defines

  def SetTargetPlatform(self, target_platform):
    self.target_platform = target_platform

  @staticmethod
  def Construct(defines, target_platform):
    node = PreprocessIfExprNode()
    node.SetDefines(defines)
    node.SetTargetPlatform(target_platform or sys.platform)
    return node


def ParseDefinesArg(definesArg):
  defines = {}
  for define_arg in definesArg:
    define, = define_arg
    parts = [part.strip() for part in define.split('=', 1)]
    name = parts[0]
    val = True if len(parts) == 1 else parts[1]
    if (val == "1" or val == "true"):
      val = True
    elif (val == "0" or val == "false"):
      val = False
    defines[name] = val
  return defines


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in-folder', required=True)
  parser.add_argument('--out-folder', required=True)
  parser.add_argument('--out-manifest')
  parser.add_argument('--in-files', required=True, nargs="*")
  parser.add_argument('-D', '--defines', nargs="*", action='append')
  parser.add_argument('-E', '--environment')
  parser.add_argument('-t', '--target')
  args = parser.parse_args(argv)

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  defines = ParseDefinesArg(args.defines)

  node = PreprocessIfExprNode.Construct(defines, args.target)

  for input_file in args.in_files:
    in_path = os.path.join(in_folder, input_file)
    content = ""
    with io.open(in_path, encoding='utf-8', mode='r') as f:
      content = f.read()

    try:
      preprocessed = node.PreprocessIfExpr(content)
    except:
      raise Exception('Error processing %s' % in_path)
    out_path = os.path.join(out_folder, input_file)
    out_dir = os.path.dirname(out_path)
    assert out_dir.startswith(out_folder), \
           'Cannot preprocess files to locations not under %s.' % out_dir
    try:
      os.makedirs(out_dir)
    except OSError as e:
      # Ignore directory exists errors. This can happen if two build rules
      # for overlapping directories hit the makedirs line at the same time.
      if e.errno != errno.EEXIST:
        raise
    if os.path.exists(out_path) and os.path.samefile(out_path, in_path):
      os.remove(out_path)

    # Detect and delete any stale TypeScript files present in the output folder,
    # corresponding to input .js files, since they can get picked up by
    # subsequent ts_library() invocations and cause transient build failures.
    # This can happen when a file is migrated from JS to TS  and a bot is
    # switched from building a later CL to building an earlier CL.
    [pathname, extension] = os.path.splitext(out_path)
    if extension == '.js':
      to_check = pathname + '.ts'
      if os.path.exists(to_check):
        os.remove(to_check)

    with io.open(out_path, mode='wb') as f:
      f.write(preprocessed.encode('utf-8'))

  if args.out_manifest:
    manifest_data = {}
    manifest_data['base_dir'] = '%s' % args.out_folder
    manifest_data['files'] = args.in_files
    manifest_file = io.open(
        os.path.normpath(os.path.join(_CWD, args.out_manifest)), 'w',
        encoding='utf-8', newline='\n')
    json.dump(manifest_data, manifest_file)
  return


if __name__ == '__main__':
  main(sys.argv[1:])
