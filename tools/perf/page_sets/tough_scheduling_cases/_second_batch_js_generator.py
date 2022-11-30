#!/usr/bin/env vpython3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import argparse
import random
import six
import sys
import zlib

# Generates synthetic Javascript for measuring the speed of parsing,
# compilation and initial execution.

# - top-level closure count
# - top-level function count
# - inner function count
# - size of code in inner functions
# - number of closures to call
# - number of top-level functions to call
# - loop count

def _ParseArguments():
  parser = argparse.ArgumentParser(
      description='Synthetic Javascript generator')
  parser.add_argument('--closure-count', metavar='N', type=int, default=1,
                      help='Number of top-level closures to generate')
  parser.add_argument('--function-count', metavar='N', type=int, default=1,
                      help='Number of top-level functions to generate')
  parser.add_argument('--inner-function-count', metavar='N', type=int,
                      default=1, help='Number of inner functions to generate')
  parser.add_argument('--inner-function-line-count', metavar='N', type=int,
                      default=1, help='Lines of code in each inner function')
  parser.add_argument('--closure-call-count', metavar='N', type=int,
                      default=1, help='Number of top-level closures to call')
  parser.add_argument('--function-call-count', metavar='N', type=int,
                      default=1, help='Number of top-level functions to call')
  parser.add_argument('--loop-count', metavar='N', type=int,
                      default=1, help='Number of top-level loop iterations')
  return parser.parse_args()


def _CreateRandomGeneratorForKey(key):
  return random.Random(zlib.crc32(key))


def _GenerateLeafFunction(out, name, line_count, indent=0):
  operations = [
    'value += 1',
    'value -= 2',
    'value *= 3',
    'value /= 4',
    'value = Math.sin(value)',
    'value = Math.pow(value, 2)',
  ]
  indent = '  ' * indent
  rand = _CreateRandomGeneratorForKey(name)
  print(indent + 'function %s(value) {' % name, file=out)
  for _ in range(line_count):
    print(indent + '  %s;' % rand.choice(operations), file=out)
  print(indent + '  return value;', file=out)
  print(indent + '}\n', file=out)


def _ClosureInnerFunctionName(closure_index, inner_index):
  return 'closure%dInnerFunction%d' % (closure_index, inner_index)


def _TopLevelClosureEntryPoint(closure_index):
  return 'closure%d' % (closure_index)


def _GenerateTopLevelClosures(
    out, count, inner_function_count, inner_function_line_count):
  for closure_index in range(count):
    print('(function() {  // closure %d' % closure_index, file=out)
    for inner_index in range(inner_function_count):
      _GenerateLeafFunction(
          out,
          _ClosureInnerFunctionName(closure_index, inner_index),
          inner_function_line_count,
          indent=1)

    print('window.%s = function(value) {' %
        _TopLevelClosureEntryPoint(closure_index), file=out)
    for inner_index in range(inner_function_count):
      print('  value = %s(value);' %
          _ClosureInnerFunctionName(closure_index, inner_index), file=out)
    print('  return value;', file=out)
    print('}', file=out)
    print('})();  // closure %d\n' % closure_index, file=out)


def _FunctionInnerFunctionName(function_index, inner_index):
  return 'function%dInnerFunction%d' % (function_index, inner_index)


def _TopLevelFunctionEntryPoint(function_index):
  return 'function%d' % (function_index)


def _GenerateTopLevelFunctions(
    out, count, inner_function_count, inner_function_line_count):
  for function_index in range(count):
    for inner_index in range(inner_function_count / 2):
      _GenerateLeafFunction(
          out,
          _FunctionInnerFunctionName(function_index, inner_index),
          inner_function_line_count)

    print('function %s(value) {' %
        _TopLevelFunctionEntryPoint(function_index), file=out)

    for inner_index in range(inner_function_count / 2, inner_function_count):
      _GenerateLeafFunction(
          out,
          _FunctionInnerFunctionName(function_index, inner_index),
          inner_function_line_count,
          indent=1)

    for inner_index in range(inner_function_count):
      print('  value = %s(value);' %
          _FunctionInnerFunctionName(function_index, inner_index), file=out)
    print('  return value;', file=out)
    print('}\n', file=out)


def _GenerateMain(out, loop_count, closure_call_count, function_call_count):
  print('function main(value) {', file=out)
  for _ in range(loop_count):
    for i in range(closure_call_count):
      print('  value = %s(value);' % _TopLevelClosureEntryPoint(i), file=out)
    for i in range(function_call_count):
      print('  value = %s(value);' % _TopLevelFunctionEntryPoint(i), file=out)

  print('  return value;', file=out)
  print('}\n', file=out)


def Main():
  args = _ParseArguments()
  out = six.StringIO()
  print('// WARNING: Generated source code. Do not edit.', file=out)
  print('//', file=out)
  print('// This file was generated with the following options:', file=out)
  print('// %s' % ' '.join(sys.argv), file=out)
  print(file=out)
  _GenerateTopLevelClosures(
      out,
      args.closure_count,
      args.inner_function_count,
      args.inner_function_line_count)
  _GenerateTopLevelFunctions(
      out,
      args.function_count,
      args.inner_function_count,
      args.inner_function_line_count)
  _GenerateMain(
      out,
      args.loop_count,
      args.closure_call_count,
      args.function_call_count)
  print(out.getvalue())


if __name__ == '__main__':
  sys.exit(Main())
