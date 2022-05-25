# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for tensor_image."""

from absl.testing import parameterized
import numpy as np
import tensorflow as tf

from tensorflow_lite_support.python.task.vision.core import color_space_type
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.task.vision.core.pybinds import image_utils
from tensorflow_lite_support.python.test import test_util


class TensorImageTest(tf.test.TestCase, parameterized.TestCase):

  def test_from_file(self):
    image_file = test_util.get_test_data_path('burger.jpg')
    image = tensor_image.TensorImage.create_from_file(image_file)
    self.assertIsInstance(image._image_data, image_utils.ImageData)
    self.assertEqual(image.height, 325)
    self.assertEqual(image.width, 480)
    self.assertEqual(image.color_space_type,
                     color_space_type.ColorSpaceType.RGB)
    self.assertIsInstance(image.buffer, np.ndarray)

  @parameterized.parameters(
      (1, color_space_type.ColorSpaceType.GRAYSCALE),
      (3, color_space_type.ColorSpaceType.RGB),
      (4, color_space_type.ColorSpaceType.RGBA),
  )
  def test_from_array(self, channels, color_type):
    height = 200
    width = 300
    array = np.random.randint(
        low=0, high=256, size=(height, width, channels), dtype=np.uint8)
    image = tensor_image.TensorImage.create_from_array(array)
    self.assertIsInstance(image._image_data, image_utils.ImageData)
    self.assertEqual(image.height, height)
    self.assertEqual(image.width, width)
    self.assertEqual(image.color_space_type, color_type)
    self.assertIsInstance(image.buffer, np.ndarray)
    self.assertAllClose(image.buffer, array)


if __name__ == '__main__':
  tf.test.main()
