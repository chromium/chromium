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

from absl.testing import parameterized
import tensorflow as tf

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import class_pb2
from tensorflow_lite_support.python.task.processor.proto import detection_options_pb2
from tensorflow_lite_support.python.task.processor.proto import detections_pb2
from tensorflow_lite_support.python.task.vision import object_detector
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_Category = class_pb2.Category
_BoundingBox = bounding_box_pb2.BoundingBox
_Detection = detections_pb2.Detection
_DetectionResult = detections_pb2.DetectionResult
_ObjectDetector = object_detector.ObjectDetector
_ObjectDetectorOptions = object_detector.ObjectDetectorOptions

_MODEL_FILE = 'coco_ssd_mobilenet_v1_1.0_quant_2018_06_29.tflite'
_IMAGE_FILE = 'cats_and_dogs.jpg'
_EXPECTED_DETECTION_RESULT = _DetectionResult(
    detections=[
        _Detection(
            bounding_box=_BoundingBox(
                origin_x=54, origin_y=396, width=393, height=196
            ),
            categories=[
                _Category(
                    index=16,
                    score=0.644531,
                    display_name='',
                    category_name='cat',
                )
            ],
        ),
        _Detection(
            bounding_box=_BoundingBox(
                origin_x=602, origin_y=157, width=394, height=447
            ),
            categories=[
                _Category(
                    index=16,
                    score=0.609375,
                    display_name='',
                    category_name='cat',
                )
            ],
        ),
        _Detection(
            bounding_box=_BoundingBox(
                origin_x=259,
                origin_y=394,
                width=181,
                height=209,
            ),
            categories=[
                _Category(
                    index=16, score=0.5625, display_name='', category_name='cat'
                )
            ],
        ),
        _Detection(
            bounding_box=_BoundingBox(
                origin_x=387,
                origin_y=197,
                width=281,
                height=409,
            ),
            categories=[
                _Category(
                    index=17,
                    score=0.5,
                    display_name='',
                    category_name='dog',
                )
            ],
        ),
    ]
)
_ALLOW_LIST = ['cat', 'dog']
_DENY_LIST = ['cat']
_SCORE_THRESHOLD = 0.3
_MAX_RESULTS = 3


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


def _create_detector_from_options(base_options, **detection_options):
  detection_options = detection_options_pb2.DetectionOptions(
      **detection_options)
  options = _ObjectDetectorOptions(
      base_options=base_options, detection_options=detection_options)
  detector = _ObjectDetector.create_from_options(options)
  return detector


class ObjectDetectorTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.model_path = test_util.get_test_data_path(_MODEL_FILE)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    detector = _ObjectDetector.create_from_file(self.model_path)
    self.assertIsInstance(detector, _ObjectDetector)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ObjectDetectorOptions(base_options=base_options)
    detector = _ObjectDetector.create_from_options(options)
    self.assertIsInstance(detector, _ObjectDetector)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name='')
      options = _ObjectDetectorOptions(base_options=base_options)
      _ObjectDetector.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, 'rb') as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _ObjectDetectorOptions(base_options=base_options)
      detector = _ObjectDetector.create_from_options(options)
      self.assertIsInstance(detector, _ObjectDetector)

  @parameterized.parameters(
      (ModelFileType.FILE_NAME, 4, _EXPECTED_DETECTION_RESULT),
      (ModelFileType.FILE_CONTENT, 4, _EXPECTED_DETECTION_RESULT))
  def test_detect_model(self, model_file_type, max_results,
                        expected_detection_result):
    # Creates detector.
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=self.model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(self.model_path, 'rb') as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError('model_file_type is invalid.')

    detector = _create_detector_from_options(
        base_options, max_results=max_results)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)

    # Comparing results.
    self.assertEqual(
        len(image_result.detections), len(expected_detection_result.detections)
    )
    for i in range(
        min(
            len(image_result.detections),
            len(expected_detection_result.detections),
        )
    ):
      self.assertEqual(
          len(image_result.detections[i].categories),
          len(expected_detection_result.detections[i].categories),
      )
      for j in range(
          min(
              len(image_result.detections[i].categories),
              len(expected_detection_result.detections[i].categories),
          )
      ):
        self.assertProtoEquals(
            image_result.detections[i].categories[j].to_pb2(),
            expected_detection_result.detections[i].categories[j].to_pb2(),
        )
      self.assertBoundingBoxApproximatelyEquals(
          image_result.detections[i].bounding_box,
          expected_detection_result.detections[i].bounding_box,
          margin=5,
      )

  def assertBoundingBoxApproximatelyEquals(
      self,
      result_bounding_box: _BoundingBox,
      expected_bounding_box: _BoundingBox,
      margin: int,
  ):
    """Verify that a bounding box is within 'margin' pixels of the expected.

    Args:
      result_bounding_box: the actual bounding box returned by the API that we
        want to test.  Each vertex of this box must be within 'margin' pixels of
        the corresponding vertex of 'expected_bounding_box'.
      expected_bounding_box: the bounding box that the test expects.
      margin: (int) the permissable error margin, in pixels.
    """
    self.assertLessEqual(
        result_bounding_box.origin_x, expected_bounding_box.origin_x + margin
    )
    self.assertGreaterEqual(
        result_bounding_box.origin_x, expected_bounding_box.origin_x - margin
    )
    self.assertLessEqual(
        result_bounding_box.origin_y, expected_bounding_box.origin_y + margin
    )
    self.assertGreaterEqual(
        result_bounding_box.origin_y, expected_bounding_box.origin_y - margin
    )
    self.assertLessEqual(
        result_bounding_box.width, expected_bounding_box.width + margin
    )
    self.assertGreaterEqual(
        result_bounding_box.width, expected_bounding_box.width - margin
    )
    self.assertLessEqual(
        result_bounding_box.height, expected_bounding_box.height + margin
    )
    self.assertGreaterEqual(
        result_bounding_box.height, expected_bounding_box.height - margin
    )

  def test_score_threshold_option(self):
    # Creates detector.
    base_options = _BaseOptions(file_name=self.model_path)
    detector = _create_detector_from_options(
        base_options, score_threshold=_SCORE_THRESHOLD)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)
    detections = image_result.detections

    for detection in detections:
      score = detection.categories[0].score
      self.assertGreaterEqual(
          score, _SCORE_THRESHOLD,
          f'Detection with score lower than threshold found. {detection}')

  def test_max_results_option(self):
    # Creates detector.
    base_options = _BaseOptions(file_name=self.model_path)
    detector = _create_detector_from_options(
        base_options, max_results=_MAX_RESULTS)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)
    detections = image_result.detections

    self.assertLessEqual(
        len(detections), _MAX_RESULTS, 'Too many results returned.')

  def test_allow_list_option(self):
    # Creates detector.
    base_options = _BaseOptions(file_name=self.model_path)
    detector = _create_detector_from_options(
        base_options, category_name_allowlist=_ALLOW_LIST)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)
    detections = image_result.detections

    for detection in detections:
      label = detection.categories[0].category_name
      self.assertIn(label, _ALLOW_LIST,
                    f'Label {label} found but not in label allow list')

  def test_deny_list_option(self):
    # Creates detector.
    base_options = _BaseOptions(file_name=self.model_path)
    detector = _create_detector_from_options(
        base_options, category_name_denylist=_DENY_LIST)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Performs object detection on the input.
    image_result = detector.detect(image)
    detections = image_result.detections

    for detection in detections:
      label = detection.categories[0].category_name
      self.assertNotIn(label, _DENY_LIST,
                       f'Label {label} found but in deny list.')

  def test_combined_allowlist_and_denylist(self):
    # Fails with combined allowlist and denylist
    with self.assertRaisesRegex(
        ValueError,
        r'`class_name_whitelist` and `class_name_blacklist` are mutually '
        r'exclusive options.'):
      base_options = _BaseOptions(file_name=self.model_path)
      detection_options = detection_options_pb2.DetectionOptions(
          category_name_allowlist=['foo'], category_name_denylist=['bar'])
      options = _ObjectDetectorOptions(
          base_options=base_options, detection_options=detection_options)
      _ObjectDetector.create_from_options(options)


if __name__ == '__main__':
  tf.test.main()
