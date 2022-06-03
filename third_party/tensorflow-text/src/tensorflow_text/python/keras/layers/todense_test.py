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

"""Tests for ToDense Keras layer."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized
import numpy as np

from tensorflow.python import keras
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import sparse_tensor
from tensorflow.python.framework import test_util
from tensorflow.python.keras import backend as K
from tensorflow.python.keras import keras_parameterized
from tensorflow.python.keras import testing_utils
from tensorflow.python.keras.engine.base_layer import Layer
from tensorflow.python.keras.layers import recurrent as rnn_v1
from tensorflow.python.keras.layers import recurrent_v2 as rnn_v2
from tensorflow.python.ops import math_ops
from tensorflow.python.ops.ragged import ragged_factory_ops
from tensorflow.python.platform import test
from tensorflow_text.python.keras.layers.todense import ToDense


class Final(Layer):
  """This is a helper layer that can be used as the last layer in a network for testing purposes."""

  def call(self, inputs):
    return math_ops.cast(inputs, dtypes.float32)

  def compute_output_shape(self, input_shape):
    return input_shape

  def get_config(self):
    base_config = super(Final, self).get_config()
    return dict(list(base_config.items()))


def get_input_dataset(in_data, out_data=None):
  batch_size = in_data.shape[0]
  if out_data is None:
    return dataset_ops.DatasetV2.from_tensor_slices(in_data).batch(batch_size)

  return dataset_ops.DatasetV2.from_tensor_slices(
      (in_data, out_data)).batch(batch_size)


@keras_parameterized.run_with_all_model_types
@keras_parameterized.run_all_keras_modes
class RaggedTensorsToDenseLayerTest(keras_parameterized.TestCase):

  def SKIP_test_ragged_input_default_padding(self):
    input_data = get_input_dataset(
        ragged_factory_ops.constant([[1, 2, 3, 4, 5], [2, 3]]))
    expected_output = np.array([[1, 2, 3, 4, 5], [2, 3, 0, 0, 0]])

    layers = [ToDense(), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_ragged=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)

  def SKIP_test_ragged_input_with_padding(self):
    input_data = get_input_dataset(
        ragged_factory_ops.constant([[[1, 2, 3, 4, 5]], [[2], [3]]]))
    expected_output = np.array([[[1., 2., 3., 4., 5.],
                                 [-1., -1., -1., -1., -1.]],
                                [[2., -1., -1., -1., -1.],
                                 [3., -1., -1., -1., -1.]]])

    layers = [ToDense(pad_value=-1), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None, None),
        input_ragged=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)

  def test_ragged_input_pad_and_mask(self):
    input_data = ragged_factory_ops.constant([[1, 2, 3, 4, 5], []])
    expected_mask = np.array([True, False])

    output = ToDense(pad_value=-1, mask=True)(input_data)
    self.assertTrue(hasattr(output, "_keras_mask"))
    self.assertIsNot(output._keras_mask, None)
    self.assertAllEqual(K.get_value(output._keras_mask), expected_mask)

  def test_ragged_input_shape(self):
    input_data = get_input_dataset(
        ragged_factory_ops.constant([[1, 2, 3, 4, 5], [2, 3]]))
    expected_output = np.array([[1, 2, 3, 4, 5, 0, 0], [2, 3, 0, 0, 0, 0, 0]])

    layers = [ToDense(shape=[2, 7]), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_ragged=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)

  @parameterized.named_parameters(
      *test_util.generate_combinations_with_testcase_name(layer=[
          rnn_v1.SimpleRNN, rnn_v1.GRU, rnn_v1.LSTM, rnn_v2.GRU, rnn_v2.LSTM
      ]))
  def SKIP_test_ragged_input_RNN_layer(self, layer):
    input_data = get_input_dataset(
        ragged_factory_ops.constant([[1, 2, 3, 4, 5], [5, 6]]))

    layers = [
        ToDense(pad_value=7, mask=True),
        keras.layers.Embedding(8, 16),
        layer(16),
        keras.layers.Dense(3, activation="softmax"),
        keras.layers.Dense(1, activation="sigmoid")
    ]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_ragged=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="rmsprop",
        loss="binary_crossentropy",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())

    output = model.predict(input_data)
    self.assertAllEqual(np.zeros((2, 1)).shape, output.shape)


@keras_parameterized.run_with_all_model_types
@keras_parameterized.run_all_keras_modes
class SparseTensorsToDenseLayerTest(keras_parameterized.TestCase):

  def SKIP_test_sparse_input_default_padding(self):
    input_data = get_input_dataset(
        sparse_tensor.SparseTensor(
            indices=[[0, 0], [1, 2]], values=[1, 2], dense_shape=[3, 4]))

    expected_output = np.array([[1., 0., 0., 0.], [0., 0., 2., 0.],
                                [0., 0., 0., 0.]])

    layers = [ToDense(), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_sparse=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)

  def SKIP_test_sparse_input_with_padding(self):
    input_data = get_input_dataset(
        sparse_tensor.SparseTensor(
            indices=[[0, 0], [1, 2]], values=[1, 2], dense_shape=[3, 4]))

    expected_output = np.array([[1., -1., -1., -1.], [-1., -1., 2., -1.],
                                [-1., -1., -1., -1.]])

    layers = [ToDense(pad_value=-1, trainable=False), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_sparse=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)

  def test_sparse_input_pad_and_mask(self):
    input_data = sparse_tensor.SparseTensor(
        indices=[[0, 0], [1, 2]], values=[1, 2], dense_shape=[3, 4])

    expected_mask = np.array([True, True, False])

    output = ToDense(pad_value=-1, mask=True)(input_data)
    self.assertTrue(hasattr(output, "_keras_mask"))
    self.assertIsNot(output._keras_mask, None)
    self.assertAllEqual(K.get_value(output._keras_mask), expected_mask)

  def test_sparse_input_shape(self):
    input_data = get_input_dataset(
        sparse_tensor.SparseTensor(
            indices=[[0, 0], [1, 2]], values=[1, 2], dense_shape=[3, 4]))

    expected_output = np.array([[1., 0., 0., 0.], [0., 0., 2., 0.],
                                [0., 0., 0., 0.]])

    layers = [ToDense(shape=[3, 4]), Final()]
    model = testing_utils.get_model_from_layers(
        layers,
        input_shape=(None,),
        input_sparse=True,
        input_dtype=dtypes.int32)
    model.compile(
        optimizer="sgd",
        loss="mse",
        metrics=["accuracy"],
        run_eagerly=testing_utils.should_run_eagerly())
    output = model.predict(input_data)
    self.assertAllEqual(output, expected_output)


if __name__ == "__main__":
  test.main()
