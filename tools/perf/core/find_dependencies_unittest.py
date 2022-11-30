# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from telemetry import decorators
from telemetry.core import util

from core import find_dependencies


class FindDependenciesTest(unittest.TestCase):
  @decorators.Disabled('chromeos')  # crbug.com/818230
  def testFindPythonDependencies(self):
    try:
      dog_object_path = os.path.join(
          util.GetUnittestDataDir(),
          'dependency_test_dir', 'dog', 'dog', 'dog_object.py')
      cat_module_path = os.path.join(
          util.GetUnittestDataDir(),
          'dependency_test_dir', 'other_animals', 'cat', 'cat')
      cat_module_init_path = os.path.join(cat_module_path, '__init__.py')
      cat_object_path = os.path.join(cat_module_path, 'cat_object.py')
      dependencies = set(
          p for p in find_dependencies.FindPythonDependencies(dog_object_path))
      self.assertEqual(dependencies, {
          dog_object_path, cat_module_path, cat_module_init_path,
          cat_object_path
      })
    except ImportError:  # crbug.com/559527
      pass

  @decorators.Disabled('chromeos')  # crbug.com/818230
  def testFindPythonDependenciesWithNestedImport(self):
    try:
      moose_module_path = os.path.join(
          util.GetUnittestDataDir(),
          'dependency_test_dir', 'other_animals', 'moose', 'moose')
      moose_object_path = os.path.join(moose_module_path, 'moose_object.py')
      horn_module_path = os.path.join(moose_module_path, 'horn')
      horn_module_init_path = os.path.join(horn_module_path, '__init__.py')
      horn_object_path = os.path.join(horn_module_path, 'horn_object.py')
      self.assertEqual(
          set(p for p in find_dependencies.FindPythonDependencies(
              moose_object_path)), {
                  moose_object_path, horn_module_path, horn_module_init_path,
                  horn_object_path
              })
    except ImportError:   # crbug.com/559527
      pass
