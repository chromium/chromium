#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cpp_bundle_generator import CppBundleGenerator
from model import Model

import json_schema
import os
import unittest


def _createCppBundleGenerator(file_path):
  json_object = json_schema.Load(file_path)
  model = Model()
  model.AddNamespace(json_object[0], file_path)
  cpp_bundle_generator = CppBundleGenerator(None, model, None, None,
                                            'generated_api_schemas', None, None,
                                            None)
  return (cpp_bundle_generator, model)


def _getPlatformIfdefs(cpp_bundle_generator, model):
  return cpp_bundle_generator._GetPlatformIfdefs(
      list(list(model.namespaces.values())[0].functions.values())[0])


class CppBundleGeneratorTest(unittest.TestCase):

  def testIfDefsForWinLinux(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_win_linux.json')
    self.assertEqual('BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)',
                     _getPlatformIfdefs(cpp_bundle_generator, model))

  def testIfDefsForAll(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_all.json')
    self.assertEqual(
        'BUILDFLAG(IS_CHROMEOS_ASH) || '
        'BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)',
        _getPlatformIfdefs(cpp_bundle_generator, model))

  def testIfDefsForChromeOS(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_chromeos.json')
    self.assertEqual('BUILDFLAG(IS_CHROMEOS_ASH)',
                     _getPlatformIfdefs(cpp_bundle_generator, model))


if __name__ == '__main__':
  unittest.main()
