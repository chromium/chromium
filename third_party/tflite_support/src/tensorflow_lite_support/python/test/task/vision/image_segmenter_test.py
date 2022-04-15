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
"""Tests for image_segmenter."""

import enum

from absl.testing import parameterized
import tensorflow as tf
from tensorflow_lite_support.python.task.core.proto import base_options_pb2
from tensorflow_lite_support.python.task.processor.proto import segmentation_options_pb2
from tensorflow_lite_support.python.task.vision import image_segmenter
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_pb2.BaseOptions
_ImageSegmenter = image_segmenter.ImageSegmenter
_ImageSegmenterOptions = image_segmenter.ImageSegmenterOptions

_MODEL_FILE = 'deeplabv3.tflite'
_IMAGE_FILE = 'segmentation_input_rotation0.jpg'
_SEGMENTATION_FILE = 'segmentation_golden_rotation0.png'
_EXPECTED_COLORED_LABELS = [{
    'r': 0,
    'g': 0,
    'b': 0,
    'class_name': 'background'
}, {
    'r': 128,
    'g': 0,
    'b': 0,
    'class_name': 'aeroplane'
}, {
    'r': 0,
    'g': 128,
    'b': 0,
    'class_name': 'bicycle'
}, {
    'r': 128,
    'g': 128,
    'b': 0,
    'class_name': 'bird'
}, {
    'r': 0,
    'g': 0,
    'b': 128,
    'class_name': 'boat'
}, {
    'r': 128,
    'g': 0,
    'b': 128,
    'class_name': 'bottle'
}, {
    'r': 0,
    'g': 128,
    'b': 128,
    'class_name': 'bus'
}, {
    'r': 128,
    'g': 128,
    'b': 128,
    'class_name': 'car'
}, {
    'r': 64,
    'g': 0,
    'b': 0,
    'class_name': 'cat'
}, {
    'r': 192,
    'g': 0,
    'b': 0,
    'class_name': 'chair'
}, {
    'r': 64,
    'g': 128,
    'b': 0,
    'class_name': 'cow'
}, {
    'r': 192,
    'g': 128,
    'b': 0,
    'class_name': 'dining table'
}, {
    'r': 64,
    'g': 0,
    'b': 128,
    'class_name': 'dog'
}, {
    'r': 192,
    'g': 0,
    'b': 128,
    'class_name': 'horse'
}, {
    'r': 64,
    'g': 128,
    'b': 128,
    'class_name': 'motorbike'
}, {
    'r': 192,
    'g': 128,
    'b': 128,
    'class_name': 'person'
}, {
    'r': 0,
    'g': 64,
    'b': 0,
    'class_name': 'potted plant'
}, {
    'r': 128,
    'g': 64,
    'b': 0,
    'class_name': 'sheep'
}, {
    'r': 0,
    'g': 192,
    'b': 0,
    'class_name': 'sofa'
}, {
    'r': 128,
    'g': 192,
    'b': 0,
    'class_name': 'train'
}, {
    'r': 0,
    'g': 64,
    'b': 128,
    'class_name': 'tv'
}]


def _create_segmenter_from_options(base_options, **segmentation_options):
  segmentation_options = segmentation_options_pb2.SegmentationOptions(
      **segmentation_options)
  options = _ImageSegmenterOptions(
      base_options=base_options, segmentation_options=segmentation_options)
  segmenter = _ImageSegmenter.create_from_options(options)
  return segmenter


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class ImageSegmenterTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.test_seg_path = test_util.get_test_data_path(_SEGMENTATION_FILE)
    self.model_path = test_util.get_test_data_path(_MODEL_FILE)

  @parameterized.parameters(
      (ModelFileType.FILE_NAME, _EXPECTED_COLORED_LABELS),
      (ModelFileType.FILE_CONTENT, _EXPECTED_COLORED_LABELS))
  def test_segment_model(self, model_file_type, expected_colored_labels):
    # Creates segmenter.
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=self.model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(self.model_path, 'rb') as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError('model_file_type is invalid.')

    segmenter = _create_segmenter_from_options(base_options)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs image segmentation on the input.
    segmentation = segmenter.segment(image).segmentation[0]
    colored_labels = segmentation.colored_labels

    # Check if the sizes of the result and expected colored labels are the same.
    self.assertEqual(
        len(colored_labels), len(expected_colored_labels),
        'Number of colored labels do not match.')

    # Comparing results.
    for index in range(len(expected_colored_labels)):
      for key in expected_colored_labels[index].keys():
        self.assertEqual(
            getattr(colored_labels[index], key),
            expected_colored_labels[index][key])


if __name__ == '__main__':
  tf.test.main()
