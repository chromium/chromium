# coding=utf-8
# Copyright 2021 TF.Text Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Test that the expected symbols are made public by tensorflow_text.

Each public module should have an _allowed_symbols attribute, listing the
public symbols for that module; and that list should match the actual list
of public symbols in that module.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import types

import tensorflow_text as tensorflow_text
from tensorflow.python.platform import test


class PublicNamesTest(test.TestCase):

  def check_names(self, module, prefix="tf_text."):
    self.assertTrue(
        hasattr(module, "_allowed_symbols"),
        "Expected to find _allowed_symbols in %s" % prefix)

    actual_symbols = set(
        name for name in module.__dict__ if not name.startswith("_"))
    missing_names = set(module._allowed_symbols) - set(actual_symbols)
    extra_names = set(actual_symbols) - set(module._allowed_symbols)

    self.assertEqual(extra_names, set(),
                     "Unexpected symbol(s) exported by %s" % prefix)
    self.assertEqual(missing_names, set(),
                     "Missing expected symbol(s) in %s" % prefix)

    for (name, value) in module.__dict__.items():
      if isinstance(value, types.ModuleType) and not name.startswith("_"):
        self.check_names(value, prefix + name + ".")

  def testPublicNames(self):
    self.check_names(tensorflow_text)


if __name__ == "__main__":
  test.main()
