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
"""Tests for tensorflow_lite_support.custom_ops.ngrams."""

import os
import sys
import timeit

from absl import logging
from absl.testing import parameterized
import tensorflow as tf
import tensorflow_text as tf_text
from tensorflow.lite.python import interpreter as interpreter_wrapper  # pylint: disable=g-direct-tensorflow-import
from tensorflow_lite_support.custom_ops.python import tflite_text_api

# Force loaded shared object symbols to be globally visible. This is needed so
# that the interpreter_wrapper, in one .so file, can see the op resolver
# in a different .so file. Note that this may already be set by default.
# pylint: disable=g-import-not-at-top,g-bad-import-order,unused-import
if hasattr(sys, 'setdlopenflags') and hasattr(sys, 'getdlopenflags'):
  sys.setdlopenflags(sys.getdlopenflags() | os.RTLD_GLOBAL)
from tensorflow_lite_support.custom_ops.kernel import _pywrap_ngrams_op_resolver

TEST_CASES = [
    [['this', 'is', 'a', 'test']],
    [['one']],
    [['two', 'tokens'], ['a', 'b']],
    [['has', 'three', 'tokens'], ['a', 'b', 'c'], ['0', '1', '2']],
    [['a', 'ragged', 'tensor'], ['a'], ['0', '1']],
    [[['a', 'multidimensional', 'test', 'case'], ['a', 'b', 'c', 'd', 'e']],
     [['0', '1', '2', '3', '4', '5']]],
]

INVOKES_FOR_SINGLE_OP_BENCHMARK = 1000
INVOKES_FOR_FLEX_DELEGATE_BENCHMARK = 100


class NgramsTest(parameterized.TestCase):

  _models = {}

  def _make_model(self, rank, width, ragged_tensor=False, flex=False):
    temp_dir = self.create_tempdir().full_path

    key = (rank, width, ragged_tensor, flex)
    if key in self._models:
      return self._models[key]

    ngrams = tf_text.ngrams if flex else tflite_text_api.ngrams

    if ragged_tensor:
      input_signature = [tf.TensorSpec(shape=[None], dtype=tf.string)]
      rs = rank - 1
      input_signature += [tf.TensorSpec(shape=[None], dtype=tf.int64)] * rs

      class Model(tf.Module):

        @tf.function(input_signature=input_signature)
        def __call__(self, values, *args):
          row_splits = list(args)
          input_tensor = tf.RaggedTensor.from_nested_row_splits(
              flat_values=values, nested_row_splits=tuple(row_splits))
          output_tensor = ngrams(
              input_tensor, width, reduction_type=tf_text.Reduction.STRING_JOIN)
          output = [output_tensor.flat_values]
          output.extend(list(output_tensor.nested_row_splits))
          return tuple(output)

      tf.saved_model.save(Model(), temp_dir)
    else:
      shape = [None] * rank

      class Model(tf.Module):

        @tf.function(
            input_signature=[tf.TensorSpec(shape=shape, dtype=tf.string)])
        def __call__(self, input_tensor):
          return ngrams(
              input_tensor, width, reduction_type=tf_text.Reduction.STRING_JOIN)

      tf.saved_model.save(Model(), temp_dir)

    converter = tf.lite.TFLiteConverter.from_saved_model(temp_dir)
    converter.inference_type = tf.float32
    converter.inference_input_type = tf.float32
    converter.allow_custom_ops = not flex
    if flex:
      converter.target_spec.supported_ops = [
          tf.lite.OpsSet.TFLITE_BUILTINS, tf.lite.OpsSet.SELECT_TF_OPS
      ]
    model = converter.convert()
    self._models[key] = model
    return model

  @parameterized.parameters([t] for t in TEST_CASES)
  def test_width_2_tensor_equivalence(self, test_case):
    input_tensor = tf.ragged.constant(test_case).to_tensor()
    tf_output = tf_text.ngrams(
        input_tensor, 2, reduction_type=tf_text.Reduction.STRING_JOIN)

    rank = input_tensor.shape.rank
    model = self._make_model(rank, 2, ragged_tensor=False, flex=False)
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model, custom_op_registerers=['AddNgramsCustomOp'])
    interpreter.resize_tensor_input(0, input_tensor.shape)
    interpreter.allocate_tensors()
    interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                           input_tensor.numpy())
    interpreter.invoke()
    tflite_output = interpreter.get_tensor(
        interpreter.get_output_details()[0]['index'])

    self.assertEqual(tf_output.numpy().tolist(), tflite_output.tolist())

  @parameterized.parameters([t] for t in TEST_CASES)
  def test_width_3_tensor_equivalence(self, test_case):
    input_tensor = tf.ragged.constant(test_case).to_tensor()
    tf_output = tf_text.ngrams(
        input_tensor, 3, reduction_type=tf_text.Reduction.STRING_JOIN)

    rank = input_tensor.shape.rank
    model = self._make_model(rank, 3, ragged_tensor=False, flex=False)
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model, custom_op_registerers=['AddNgramsCustomOp'])
    interpreter.resize_tensor_input(0, input_tensor.shape)
    interpreter.allocate_tensors()
    interpreter.set_tensor(interpreter.get_input_details()[0]['index'],
                           input_tensor.numpy())
    interpreter.invoke()
    tflite_output = interpreter.get_tensor(
        interpreter.get_output_details()[0]['index'])
    self.assertEqual(tf_output.numpy().tolist(), tflite_output.tolist())

  @parameterized.parameters([t] for t in TEST_CASES)
  def test_width_2_ragged_tensor_equivalence(self, test_case):
    input_tensor = tf.ragged.constant(test_case)
    tf_output = tf_text.ngrams(
        input_tensor, 2, reduction_type=tf_text.Reduction.STRING_JOIN)
    rank = input_tensor.shape.rank
    model = self._make_model(rank, 2, ragged_tensor=True, flex=False)
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model, custom_op_registerers=['AddNgramsCustomOp'])
    signature_fn = interpreter.get_signature_runner()
    signature_kwargs = {}
    signature_kwargs['values'] = input_tensor.flat_values.numpy()
    for r in range(rank - 1):
      signature_kwargs[f'args_{r}'] = input_tensor.nested_row_splits[r].numpy()
    output = signature_fn(**signature_kwargs)
    tflite_output_values = output['output_0']
    self.assertEqual(tf_output.flat_values.numpy().tolist(),
                     tflite_output_values.tolist())
    for i in range(rank - 1):
      tflite_output_cur_row_splits = output[f'output_{i + 1}']
      self.assertEqual(tf_output.nested_row_splits[i].numpy().tolist(),
                       tflite_output_cur_row_splits.tolist())

  @parameterized.parameters([t] for t in TEST_CASES)
  def test_width_3_ragged_tensor_equivalence(self, test_case):
    input_tensor = tf.ragged.constant(test_case)
    tf_output = tf_text.ngrams(
        input_tensor, 3, reduction_type=tf_text.Reduction.STRING_JOIN)

    rank = input_tensor.shape.rank
    model = self._make_model(rank, 3, ragged_tensor=True, flex=False)
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=model, custom_op_registerers=['AddNgramsCustomOp'])
    signature_fn = interpreter.get_signature_runner()
    signature_kwargs = {}
    signature_kwargs['values'] = (
        input_tensor.flat_values.numpy().astype('bytes'))
    for r in range(rank - 1):
      signature_kwargs[f'args_{r}'] = input_tensor.nested_row_splits[r].numpy()
    output = signature_fn(**signature_kwargs)
    tflite_output_values = output['output_0']
    self.assertEqual(tf_output.flat_values.numpy().tolist(),
                     tflite_output_values.tolist())
    for i in range(rank - 1):
      tflite_output_cur_row_splits = output[f'output_{i+1}']
      self.assertEqual(tf_output.nested_row_splits[i].numpy().tolist(),
                       tflite_output_cur_row_splits.tolist())

  def test_latency(self):
    latency_op = 0.0
    for test_case in TEST_CASES:
      input_tensor = tf.ragged.constant(test_case)

      rank = input_tensor.shape.rank
      model = self._make_model(rank, 3, ragged_tensor=True, flex=False)
      interpreter = interpreter_wrapper.InterpreterWithCustomOps(
          model_content=model, custom_op_registerers=['AddNgramsCustomOp'])
    signature_fn = interpreter.get_signature_runner()
    signature_kwargs = {}
    signature_kwargs['values'] = input_tensor.flat_values.numpy()
    for r in range(rank - 1):
      signature_kwargs[f'args_{r}'] = input_tensor.nested_row_splits[r].numpy()
    start_time = timeit.default_timer()
    for _ in range(INVOKES_FOR_SINGLE_OP_BENCHMARK):
      _ = signature_fn(**signature_kwargs)
      latency_op = latency_op + timeit.default_timer() - start_time
    latency_op = latency_op / (
        INVOKES_FOR_SINGLE_OP_BENCHMARK * len(TEST_CASES))

    latency_flex = 0.0
    for test_case in TEST_CASES:
      input_tensor = tf.ragged.constant(test_case)

      rank = input_tensor.shape.rank
      model = self._make_model(rank, 3, ragged_tensor=True, flex=True)
      interpreter = interpreter_wrapper.Interpreter(model_content=model)
      signature_fn = interpreter.get_signature_runner()
      signature_kwargs = {}
      signature_kwargs['values'] = input_tensor.flat_values.numpy()

      for r in range(rank - 1):
        signature_kwargs[f'args_{r}'] = input_tensor.nested_row_splits[r].numpy(
        )
      start_time = timeit.default_timer()
      for _ in range(INVOKES_FOR_FLEX_DELEGATE_BENCHMARK):
        _ = signature_fn(**signature_kwargs)
        latency_flex = latency_flex + timeit.default_timer() - start_time
    latency_flex = latency_flex / (
        INVOKES_FOR_FLEX_DELEGATE_BENCHMARK * len(TEST_CASES))

    logging.info('Latency (single op): %fms', latency_op * 1000.0)
    logging.info('Latency (flex delegate): %fms', latency_flex * 1000.0)


if __name__ == '__main__':
  tf.test.main()
