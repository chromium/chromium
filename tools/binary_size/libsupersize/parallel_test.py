#!/usr/bin/env python3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import threading
import unittest

import parallel


def _ForkTestHelper(arg1, arg2, pickle_me_not, test_instance, parent_pid):
  _ = pickle_me_not  # Suppress lint warning.
  test_instance.assertNotEqual(os.getpid(), parent_pid)
  return arg1 + arg2


class Unpicklable:
  """Ensures that pickle() is not called on parameters."""

  def __getstate__(self):
    raise AssertionError('Tried to pickle')


class ConcurrentTest(unittest.TestCase):
  def testEncodeDictOfLists_Empty(self):
    test_dict = {}
    encoded = parallel.EncodeDictOfLists(test_dict)
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(test_dict, decoded)

  def testEncodeDictOfLists_EmptyValue(self):
    test_dict = {'foo': []}
    encoded = parallel.EncodeDictOfLists(test_dict)
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(test_dict, decoded)

  def testEncodeDictOfLists_AllStrings(self):
    test_dict = {'foo': ['a', 'b', 'c'], 'foo2': ['a', 'b']}
    encoded = parallel.EncodeDictOfLists(test_dict)
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(test_dict, decoded)

  def testEncodeDictOfLists_KeyTransform(self):
    test_dict = {0: ['a', 'b', 'c'], 9: ['a', 'b']}
    encoded = parallel.EncodeDictOfLists(test_dict, key_transform=str)
    decoded = parallel.DecodeDictOfLists(encoded, key_transform=int)
    self.assertEqual(test_dict, decoded)

  def testEncodeDictOfLists_ValueTransform(self):
    test_dict = {'a': ['0', '1', '2'], 'b': ['3', '4']}
    expected = {'a': [0, 1, 2], 'b': [3, 4]}
    encoded = parallel.EncodeDictOfLists(test_dict)
    decoded = parallel.DecodeDictOfLists(encoded, value_transform=int)
    self.assertEqual(expected, decoded)

  def testEncodeDictOfLists_Join_Empty(self):
    test_dict1 = {}
    test_dict2 = {}
    expected = {}
    encoded1 = parallel.EncodeDictOfLists(test_dict1)
    encoded2 = parallel.EncodeDictOfLists(test_dict2)
    encoded = parallel.JoinEncodedDictOfLists([encoded1, encoded2])
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(expected, decoded)

  def testEncodeDictOfLists_Join_Singl(self):
    test_dict1 = {'key1': ['a']}
    encoded1 = parallel.EncodeDictOfLists(test_dict1)
    encoded = parallel.JoinEncodedDictOfLists([encoded1])
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(test_dict1, decoded)

  def testEncodeDictOfLists_JoinMultiple(self):
    test_dict1 = {'key1': ['a']}
    test_dict2 = {'key2': ['b']}
    expected = {'key1': ['a'], 'key2': ['b']}
    encoded1 = parallel.EncodeDictOfLists(test_dict1)
    encoded2 = parallel.EncodeDictOfLists({})
    encoded3 = parallel.EncodeDictOfLists(test_dict2)
    encoded = parallel.JoinEncodedDictOfLists([encoded1, encoded2, encoded3])
    decoded = parallel.DecodeDictOfLists(encoded)
    self.assertEqual(expected, decoded)

  def testCallOnThread(self):
    main_thread = threading.current_thread()

    def callback(arg1, arg2):
      self.assertEqual(1, arg1)
      self.assertEqual(2, arg2)
      my_thread = threading.current_thread()
      self.assertNotEqual(my_thread, main_thread)
      return 3

    result = parallel.CallOnThread(callback, 1, arg2=2)
    self.assertEqual(3, result.get())

  def testForkAndCall_normal(self):
    parent_pid = os.getpid()
    result = parallel.ForkAndCall(_ForkTestHelper,
                                  (1, 2, Unpicklable(), self, parent_pid))
    self.assertEqual(3, result.get())

  def testForkAndCall_exception(self):
    parent_pid = os.getpid()
    result = parallel.ForkAndCall(_ForkTestHelper,
                                  (1, 'a', None, self, parent_pid))
    self.assertRaises(TypeError, result.get)

  def testBulkForkAndCall_none(self):
    results = parallel.BulkForkAndCall(_ForkTestHelper, [])
    self.assertEqual([], list(results))

  def testBulkForkAndCall_few(self):
    parent_pid = os.getpid()
    results = parallel.BulkForkAndCall(_ForkTestHelper,
                                       [(1, 2, Unpicklable(), self, parent_pid),
                                        (3, 4, None, self, parent_pid)])
    self.assertEqual({3, 7}, set(results))

  def testBulkForkAndCall_few_kwargs(self):
    parent_pid = os.getpid()
    results = parallel.BulkForkAndCall(
        _ForkTestHelper, [(1, 2, Unpicklable()), (3, 4, None)],
        test_instance=self,
        parent_pid=parent_pid)
    self.assertEqual({3, 7}, set(results))

  def testBulkForkAndCall_many(self):
    parent_pid = os.getpid()
    args = [(1, 2, Unpicklable(), self, parent_pid) for _ in range(100)]
    results = parallel.BulkForkAndCall(_ForkTestHelper, args)
    self.assertEqual([3] * 100, list(results))

  def testBulkForkAndCall_many_kwargs(self):
    parent_pid = os.getpid()
    args = [(1, 2) for _ in range(100)]
    results = parallel.BulkForkAndCall(
        _ForkTestHelper,
        args,
        pickle_me_not=Unpicklable(),
        test_instance=self,
        parent_pid=parent_pid)
    self.assertEqual([3] * 100, list(results))

  def testBulkForkAndCall_exception(self):
    parent_pid = os.getpid()
    results = parallel.BulkForkAndCall(_ForkTestHelper,
                                       [(1, 'a', None, self, parent_pid)])
    self.assertRaises(TypeError, results.__next__)


if __name__ == '__main__':
  unittest.main()
