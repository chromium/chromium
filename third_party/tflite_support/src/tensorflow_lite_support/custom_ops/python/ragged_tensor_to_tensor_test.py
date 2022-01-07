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
# =============================================================================
"""Tests for ragged_tensor_to_tensor."""

import tensorflow as tf
from tensorflow.lite.python import interpreter as interpreter_wrapper  # pylint: disable=g-direct-tensorflow-import


class RaggedTensorToTensorTest(tf.test.TestCase):

  def test_ragged_to_tensor(self):

    @tf.function
    def ragged_tensor_function():
      ragged_tensor = tf.RaggedTensor.from_row_splits(
          values=[
              13, 36, 83, 131, 13, 36, 4, 3127, 152, 130, 30, 2424, 168, 1644,
              1524, 4, 3127, 152, 130, 30, 2424, 168, 1644, 636
          ],
          row_splits=[0, 0, 6, 15, 24])
      return ragged_tensor.to_tensor()

    concrete_function = ragged_tensor_function.get_concrete_function()

    converter = tf.lite.TFLiteConverter.from_concrete_functions(
        [concrete_function], ragged_tensor_function)
    converter.allow_custom_ops = True
    tflite_model = converter.convert()
    interpreter = interpreter_wrapper.InterpreterWithCustomOps(
        model_content=tflite_model,
        custom_op_registerers=["TFLite_RaggedTensorToTensorRegisterer"])
    interpreter.allocate_tensors()
    interpreter.invoke()
    output_details = interpreter.get_output_details()
    expected_result_values = [[0, 0, 0, 0, 0, 0, 0, 0, 0],
                              [13, 36, 83, 131, 13, 36, 0, 0, 0],
                              [4, 3127, 152, 130, 30, 2424, 168, 1644, 1524],
                              [4, 3127, 152, 130, 30, 2424, 168, 1644, 636]]
    self.assertAllEqual(
        interpreter.get_tensor(output_details[0]["index"]),
        expected_result_values)


if __name__ == "__main__":
  tf.test.main()
