#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import argparse
import os
import subprocess
import sys
import shutil

script_dir = os.path.dirname(os.path.realpath(__file__))
tool_dir = os.path.abspath(os.path.join(script_dir, '../../pylib'))
sys.path.insert(0, tool_dir)

from clang import plugin_testing

class StackMapTest(plugin_testing.ClangPluginTest):
  """Test harness for stack map artefact."""

  def __init__(self, test_base, llvm_bin_path, libgc_path, ident_sp_pass_path,
               reg_gc_pass_path,):
    self._test_base = test_base
    self._llvm_bin_path = llvm_bin_path
    self._libgc_path = libgc_path
    self._ident_sp_pass_path = ident_sp_pass_path
    self._reg_gc_pass_path = reg_gc_pass_path

    self._clang_path = os.path.join(llvm_bin_path, 'clang++')
    self._opt_path = os.path.join(llvm_bin_path, 'opt')
    self._llc_path = os.path.join(llvm_bin_path, 'llc')
    self._out_dir = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 'out')

  def build_commands(self, test_name):
      ll_filename = os.path.join(self._out_dir, "%s.ll" % test_name)
      ll_with_gc_filename = os.path.join(
          self._out_dir, "%s_optimised.ll" % test_name)
      asm_filename = os.path.join(self._out_dir, "%s.s" % test_name)
      obj_filename = os.path.join(self._out_dir, "%s.o" % test_name)
      bin_name = os.path.join(self._out_dir, "%s.out" % test_name)

      # Run the clang++ frontend with -O2 but stop after emitting the IR. There
      # is a bug in clang which prevents us from running GC related IR phases
      # directly from the frontend and requires us to split it into multiple
      # build phases.
      clang_cmd = [
          self._clang_path,
          '-std=c++14',
          '-fno-omit-frame-pointer',
          '-I../',
          '-Xclang',
          '-load',
          '-Xclang',
          self._ident_sp_pass_path,
          '-O2',
          '-S',
          '-emit-llvm',
          '-o', ll_filename,
          '%s.cpp' % test_name
      ]

      # Run two passes on the IR. The first selects which functions will be
      # safepointed, the second inserts statepoint relocation sequences and ends
      # up in stack maps being generated during the lowering phase.
      opt_cmd = [
          self._opt_path,
          '-load=%s' % self._reg_gc_pass_path,
          '-register-gc-fns',
          '-rewrite-statepoints-for-gc',
          '-S',
          '-o', ll_with_gc_filename,
          ll_filename
      ]

      # Note: We must ensure each stage of lowering disables omit frame pointer
      # optimisation
      llc_cmd = [
          self._llc_path,
          ll_with_gc_filename,
          '--frame-pointer=all',
          '-o',
          asm_filename
      ]

      # The next two stages are required because ToT LLVM emits stackmaps which
      # are local only to their object file. In a somewhat hacky fix, we
      # globalise this symbol so that it can be used by the independent GC
      # runtime library.
      make_native = [
          self._clang_path,
          '-c',
          '-o',
          obj_filename,
          asm_filename,
      ]

      obj_copy = [
          'objcopy',
          '--globalize-symbol=__LLVM_StackMaps',
          obj_filename
      ]

      # Link the GC runtime and create target executable
      link_cmd = [
          self._clang_path,
          obj_filename,
          '-fno-omit-frame-pointer',
          self._libgc_path,
          '-o',
          bin_name
      ]

      run_cmd = ['%s' % bin_name]
      return [
          clang_cmd,
          opt_cmd,
          llc_cmd,
          make_native,
          obj_copy,
          link_cmd,
          run_cmd
      ]

  def Run(self):
    """Runs the tests.

    The working directory is temporarily changed to self._test_base while
    running the tests.

    Returns: the number of failing tests.
    """
    print('Using llvm tools in %s...' % self._llvm_bin_path)

    os.chdir(self._test_base)

    passing = []
    failing = []
    tests = glob.glob('*.cpp')

    # Delete out directory if it already exists
    if (os.path.exists(self._out_dir)):
      shutil.rmtree(self._out_dir)

    os.mkdir(self._out_dir)
    for test in tests:
      sys.stdout.write('Testing %s...' % test)
      test_name, _ = os.path.splitext(test)

      cmds = self.build_commands(test_name)
      failure_message = self.RunOneTest(test_name, cmds)

      if failure_message:
        print('\n\tfailed: %s' % failure_message)
        failing.append(test_name)
      else:
        print('\tpassed!')
        passing.append(test_name)

    print('Ran %d tests: %d succeeded, %d failed' % (
        len(passing) + len(failing), len(passing), len(failing)))
    for test in failing:
      print('    %s' % test)
    return len(failing)

  def RunOneTest(self, test_name, cmds):
    for cmd in cmds:
      try:
        failure_message = ""
        subprocess.check_output(cmd, stderr=subprocess.STDOUT)
      except subprocess.CalledProcessError as e:
        failure_message = e.output
        break
      except Exception as e:
        return 'could not execute %s (%s)' % (cmd, e)

    return self.ProcessOneResult(test_name, failure_message)

  def ProcessOneResult(self, test_name, failure_message):
    if failure_message:
      return failure_message.replace('\r\n', '\n')

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'llvm_bin_path', help='The path to the llvm tools bin dir.')
  parser.add_argument('libgc_path', help='The path to the runtime gc library.')
  parser.add_argument('identify_safepoints_path',
    help='The path to the identify safepoints IR pass.')
  parser.add_argument('reg_gc_fns_path',
    help='The path to the register GC functions IR pass.')
  args = parser.parse_args()

  return StackMapTest(
      os.path.dirname(os.path.realpath(__file__)),
      args.llvm_bin_path,
      args.libgc_path,
      args.identify_safepoints_path,
      args.reg_gc_fns_path,
      ).Run()

if __name__ == '__main__':
  sys.exit(main())
