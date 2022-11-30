# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
# ==============================================================================
"""Tests for tensorflow_lite_support.custom_ops.kernel.whitespace_tokenizer."""

import os
import sys
import timeit

from absl import logging
from absl.testing import parameterized
import numpy as np
import tensorflow as tf
import tensorflow_text as tf_text
# pylint: disable=g-direct-tensorflow-import
from tensorflow.lite.python import interpreter as interpreter_wrapper
from tensorflow.python.platform import resource_loader

# Force loaded shared object symbols to be globally visible. This is needed so
# that the interpreter_wrapper, in one .so file, can see the op resolver
# in a different .so file. Note that this may already be set by default.
# pylint: disable=g-import-not-at-top,g-bad-import-order,unused-import
if hasattr(sys, 'setdlopenflags') and hasattr(sys, 'getdlopenflags'):
  sys.setdlopenflags(sys.getdlopenflags() | os.RTLD_GLOBAL)
from tensorflow_lite_support.custom_ops.kernel import _pywrap_whitespace_tokenizer_op_resolver

TEST_CASES = [
    ['this is a test'],
    ['extra   spaces    in     here'],
    ['a four token sentence', 'a five token sentence thing.'],
    [['a multi dimensional test case', 'a b c d', 'e f g'],
     ['h i j', 'k l m 2 3', 'n o p'], ['q r s 0 1', 't u v', 'w x y z']],
]

INVOKES_FOR_SINGLE_OP_BENCHMARK = 1000
INVOKES_FOR_FLEX_DELEGATE_BENCHMARK = 10


@tf.function
def _call_whitespace_tokenizer_to_tensor(test_case):
  tokenizer = tf_text.WhitespaceTokenizer()
  return tokenizer.tokenize(test_case).to_tensor()


@tf.function
def _call_whitespace_tokenizer_to_ragged(test_case):
  tokenizer = tf_text.WhitespaceTokenizer()
  return tokenizer.tokenize(test_case)


class WhitespaceTokenizerTest(parameterized.TestCase):

  @parameterized.parameters([t] for t in TEST_CASES)
  def testToTensorEquivalence(self, test_case):
    tf_output = _call_whitespace_tokenizer_to_tensor(test_case)

    model_filename = resource_loader.get_path_to_datafile(
        'testdata/whitespace_tokenizer_to_tensor.tflite')
    with open(model_filename, 'rb') as file:
      model = file.read()
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model,
        custom_op_registerers=['AddWhitespaceTokenizerCustomOp'])

    np_test_case = np.array(test_case, dtype=np.str)
    interpreter.resize_tensor_input(0, np_test_case.shape)
    interpreter.allocate_tensors()
    interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                           np_test_case)
    interpreter.invoke()
    tflite_output = interpreter.get_tensor(
        interpreter.get_output_details()[0]['index'])

    self.assertEqual(tf_output.numpy().tolist(), tflite_output.tolist())

  @parameterized.parameters([t] for t in TEST_CASES)
  def testToRaggedEquivalence(self, test_case):
    tf_output = _call_whitespace_tokenizer_to_ragged(test_case)

    np_test_case = np.array(test_case, dtype=np.str)
    rank = len(np_test_case.shape)

    model_filename = resource_loader.get_path_to_datafile(
        'testdata/whitespace_tokenizer_to_ragged_{}d_input.tflite'.format(rank))
    with open(model_filename, 'rb') as file:
      model = file.read()
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model,
        custom_op_registerers=['AddWhitespaceTokenizerCustomOp'])
    interpreter.resize_tensor_input(0, np_test_case.shape)
    interpreter.allocate_tensors()
    interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                           np_test_case)
    interpreter.invoke()

    # Traverse the nested row_splits/values of the ragged tensor.
    for i in range(rank):
      tflite_output_cur_row_splits = interpreter.get_tensor(
          interpreter.get_output_details()[1 + i]['index'])
      self.assertEqual(tf_output.row_splits.numpy().tolist(),
                       tflite_output_cur_row_splits.tolist())
      tf_output = tf_output.values

    tflite_output_values = interpreter.get_tensor(
        interpreter.get_output_details()[0]['index'])
    self.assertEqual(tf_output.numpy().tolist(), tflite_output_values.tolist())

  def testSingleOpLatency(self):
    model_filename = resource_loader.get_path_to_datafile(
        'testdata/whitespace_tokenizer_to_tensor.tflite')
    with open(model_filename, 'rb') as file:
      model = file.read()
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model,
        custom_op_registerers=['AddWhitespaceTokenizerCustomOp'])

    latency = 0.0
    for test_case in TEST_CASES:
      np_test_case = np.array(test_case, dtype=np.str)
      interpreter.resize_tensor_input(0, np_test_case.shape)
      interpreter.allocate_tensors()
      interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                             np_test_case)
      start_time = timeit.default_timer()
      for _ in range(INVOKES_FOR_SINGLE_OP_BENCHMARK):
        interpreter.invoke()
      latency = latency + timeit.default_timer() - start_time

    latency = latency / (INVOKES_FOR_SINGLE_OP_BENCHMARK * len(TEST_CASES))
    logging.info('Latency: %fms', latency * 1000.0)

  def testFlexDelegateLatency(self):
    model_filename = resource_loader.get_path_to_datafile(
        'testdata/whitespace_tokenizer_flex_delegate.tflite')
    with open(model_filename, 'rb') as file:
      model = file.read()
    interpreter = interpreter_wrapper.Interpreter(model_content=model)

    latency = 0.0
    for test_case in TEST_CASES:
      np_test_case = np.array(test_case, dtype=np.str)
      interpreter.resize_tensor_input(0, np_test_case.shape)
      interpreter.allocate_tensors()
      interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                             np_test_case)
      start_time = timeit.default_timer()
      for _ in range(INVOKES_FOR_FLEX_DELEGATE_BENCHMARK):
        interpreter.invoke()
      latency = latency + timeit.default_timer() - start_time

    latency = latency / (INVOKES_FOR_FLEX_DELEGATE_BENCHMARK * len(TEST_CASES))
    logging.info('Latency: %fms', latency * 1000.0)


if __name__ == '__main__':
  tf.test.main()
