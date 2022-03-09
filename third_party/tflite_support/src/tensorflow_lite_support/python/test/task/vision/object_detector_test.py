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
"""Tests for object detector."""

import enum
import json

from absl.testing import parameterized
# TODO(b/220067158): Change to import tensorflow and leverage tf.test once
# fixed the dependency issue.
from google.protobuf import json_format
import unittest
from tensorflow_lite_support.python.task.core import task_options
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import class_pb2
from tensorflow_lite_support.python.task.processor.proto import detection_options_pb2
from tensorflow_lite_support.python.task.processor.proto import detections_pb2
from tensorflow_lite_support.python.task.vision import object_detector
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import base_test
from tensorflow_lite_support.python.test import test_util

_BaseOptions = task_options.BaseOptions
_ExternalFile = task_options.ExternalFile
_ObjectDetector = object_detector.ObjectDetector
_ObjectDetectorOptions = object_detector.ObjectDetectorOptions

_MODEL_FILE = 'coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.tflite'
_IMAGE_FILE = 'cats_and_dogs.jpg'
_ACCEPTABLE_ERROR_RANGE = 0.000001


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class ObjectDetectorTest(parameterized.TestCase, base_test.BaseTestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.model_path = test_util.get_test_data_path(_MODEL_FILE)

  @classmethod
  def create_detector_from_options(cls, model_file, **detection_options):
    print(detection_options)
    base_options = _BaseOptions(model_file=model_file)
    detection_options = detection_options_pb2.DetectionOptions(
        **detection_options)
    options = _ObjectDetectorOptions(
        base_options=base_options, detection_options=detection_options)
    detector = _ObjectDetector.create_from_options(options)
    return detector

  @classmethod
  def build_test_data(cls, expected_detections):
    expected_result = detections_pb2.DetectionResult()

    for index in range(len(expected_detections)):
      bounding_box, category = expected_detections[index]
      detection = detections_pb2.Detection()
      detection.bounding_box.CopyFrom(
          bounding_box_pb2.BoundingBox(**bounding_box))
      detection.classes.append(class_pb2.Category(**category))
      expected_result.detections.append(detection)

    expected_result_dict = json.loads(
        json_format.MessageToJson(expected_result))

    return expected_result_dict

  def test_top1_result(self):
    # Creates detector.
    model_file = _ExternalFile(file_name=self.model_path)

    detector = self.create_detector_from_options(
        model_file=model_file, max_results=1)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    # Builds test data.
    expected_detections = [({
        'origin_x': 54,
        'origin_y': 396,
        'width': 393,
        'height': 196
    }, {
        'index': 16,
        'score': 0.64453125,
        'class_name': 'cat'
    })]
    expected_result_dict = self.build_test_data(expected_detections)

    # Comparing results.
    self.assertDeepAlmostEqual(
        image_result_dict, expected_result_dict, delta=_ACCEPTABLE_ERROR_RANGE)


if __name__ == '__main__':
  unittest.main()
