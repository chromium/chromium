# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
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
"""Base TestCase for the unit tests."""

import unittest

__unittest = True  # Allows shorter stack trace for .assertDeepAlmostEqual  pylint: disable=invalid-name


class BaseTestCase(unittest.TestCase):
  """Base test case."""

  def assertDeepAlmostEqual(self, expected, actual, **kwargs):
    """Compares lists, dicts and tuples recursively.

    Checks numeric values using test_case's
    :py:meth:`unittest.TestCase.assertAlmostEqual` and checks all other values
    with :py:meth:`unittest.TestCase.assertEqual`. Accepts additional keyword
    arguments and pass those intact to assertAlmostEqual() (that's how you
    specify comparison precision).

    Args:
      expected: Expected object.
      actual: Actual object.
      **kwargs: Other parameters to be passed.
    """
    if isinstance(expected, (int, float, complex)):
      self.assertAlmostEqual(expected, actual, **kwargs)
    elif isinstance(expected, (list, tuple)):
      self.assertEqual(len(expected), len(actual))
      for index in range(len(expected)):
        v1, v2 = expected[index], actual[index]
        self.assertDeepAlmostEqual(v1, v2, **kwargs)
    elif isinstance(expected, dict):
      self.assertEqual(set(expected), set(actual))
      for key in expected:
        self.assertDeepAlmostEqual(expected[key], actual[key], **kwargs)
    else:
      self.assertEqual(expected, actual)
