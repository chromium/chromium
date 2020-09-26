# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import errno
import io
import json
import os
import sys

import grit.format.html_inline
import grit.node.base
import grit.util

_CWD = os.getcwd()


class PreprocessNode(grit.node.base.Node):
  def __init__(self):
    super(PreprocessNode, self).__init__()

  def ProcessFile(self, filepath):
    return grit.format.html_inline.InlineToString(
        filepath,
        self,
        preprocess_only=True,
        allow_external_script=False,
        strip_whitespace=False,
        rewrite_function=None,
        filename_expansion_function=None)

  def EvaluateCondition(self, expr):
    return grit.node.base.Node.EvaluateExpression(expr, self.defines,
                                                  self.target_platform, {})

  def SetDefines(self, defines):
    self.defines = defines

  def SetTargetPlatform(self, target_platform):
    self.target_platform = target_platform

  @staticmethod
  def Construct(defines, target_platform):
    node = PreprocessNode()
    node.SetDefines(defines)
    node.SetTargetPlatform(target_platform or sys.platform)
    return node


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

  defines = {}
  for define_arg in args.defines:
    define, = define_arg
    name, val = grit.util.ParseDefine(define)
    defines[name] = val

  node = PreprocessNode.Construct(defines, args.target)

  for input_file in args.in_files:
    output = node.ProcessFile(os.path.join(in_folder, input_file))

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
    with io.open(out_path, mode='wb') as f:
      f.write(output.encode('utf-8'))

  if args.out_manifest:
    manifest_data = {}
    manifest_data['base_dir'] = '%s' % args.out_folder
    manifest_data['files'] = args.in_files
    manifest_file = open(
        os.path.normpath(os.path.join(_CWD, args.out_manifest)), 'wb')
    json.dump(manifest_data, manifest_file)
  return


if __name__ == '__main__':
  main(sys.argv[1:])
