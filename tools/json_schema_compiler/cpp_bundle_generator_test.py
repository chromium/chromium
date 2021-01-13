#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
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
  cpp_bundle_generator = CppBundleGenerator(
      None, model, None, None, 'generated_api_schemas',
      None, None, None)
  return (cpp_bundle_generator, model)

def _getPlatformIfdefs(cpp_bundle_generator, model):
  return cpp_bundle_generator._GetPlatformIfdefs(
      model.namespaces.values()[0].functions.values()[0])

class CppBundleGeneratorTest(unittest.TestCase):
  def testIfDefsForWinLinux(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_win_linux.json')
    self.assertEquals(
        'defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))',
        _getPlatformIfdefs(cpp_bundle_generator, model))

  def testIfDefsForAll(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_all.json')
    self.assertEquals(
        'defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || '
        '(defined(OS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_LACROS))',
        _getPlatformIfdefs(cpp_bundle_generator, model))

  def testIfDefsForChromeOS(self):
    cpp_bundle_generator, model = _createCppBundleGenerator(
        'test/function_platform_chromeos.json')
    self.assertEquals('(defined(OS_CHROMEOS) && '
                      '!BUILDFLAG(IS_CHROMEOS_LACROS))',
                      _getPlatformIfdefs(cpp_bundle_generator, model))


if __name__ == '__main__':
  unittest.main()
