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

# -*- coding: utf-8 -*-
"""Tests for Utf8Chars Op from string_ops."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.platform import test
from tensorflow_text.python.ops import string_ops


class CoerceToUtf8Test(test.TestCase):

  def testCoercetoStructurallyValidOnValidInput(self):
    with self.test_session():
      utf8 = string_ops.coerce_to_structurally_valid_utf8(["abc"])
    self.assertAllEqual(utf8, [b"abc"])

  def testCoercetoStructurallyValidOnValidInputWithDefault(self):
    with self.test_session():
      utf8 = string_ops.coerce_to_structurally_valid_utf8(["abc"], "?")
    self.assertAllEqual(utf8, [b"abc"])

  def testCoercetoStructurallyValidOnInvalidInput(self):
    with self.test_session():
      utf8 = string_ops.coerce_to_structurally_valid_utf8([b"abc\xfd"])
    self.assertAllEqual(utf8, [u"abc�".encode("utf-8")])

  def testCoercetoStructurallyValidOnInvalidInputWithDefault(self):
    with self.test_session():
      utf8 = string_ops.coerce_to_structurally_valid_utf8([b"abc\xfd"], "?")
    self.assertAllEqual(utf8, [b"abc?"])


if __name__ == "__main__":
  test.main()
