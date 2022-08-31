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
"""Tests for image_classifier."""

import enum

from absl.testing import parameterized
import tensorflow as tf

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import class_pb2
from tensorflow_lite_support.python.task.processor.proto import classification_options_pb2
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.vision import image_classifier
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_Category = class_pb2.Category
_Classifications = classifications_pb2.Classifications
_ClassificationResult = classifications_pb2.ClassificationResult
_ImageClassifier = image_classifier.ImageClassifier
_ImageClassifierOptions = image_classifier.ImageClassifierOptions

_MODEL_FILE = 'mobilenet_v2_1.0_224.tflite'
_IMAGE_FILE = 'burger.jpg'
_EXPECTED_CLASSIFICATION_RESULT = _ClassificationResult(classifications=[
    _Classifications(
        categories=[
            _Category(
                index=934,
                score=0.739974,
                display_name='',
                category_name='cheeseburger'),
            _Category(
                index=925,
                score=0.026929,
                display_name='',
                category_name='guacamole'),
            _Category(
                index=932,
                score=0.025737,
                display_name='',
                category_name='bagel')
        ],
        head_index=0,
        head_name='')
])
_ALLOW_LIST = ['cheeseburger', 'guacamole']
_DENY_LIST = ['cheeseburger']
_SCORE_THRESHOLD = 0.5
_MAX_RESULTS = 3


def _create_classifier_from_options(base_options, **classification_options):
  classification_options = classification_options_pb2.ClassificationOptions(
      **classification_options)
  options = _ImageClassifierOptions(
      base_options=base_options, classification_options=classification_options)
  classifier = _ImageClassifier.create_from_options(options)
  return classifier


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class ImageClassifierTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.model_path = test_util.get_test_data_path(_MODEL_FILE)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    classifier = _ImageClassifier.create_from_file(self.model_path)
    self.assertIsInstance(classifier, _ImageClassifier)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ImageClassifierOptions(base_options=base_options)
    classifier = _ImageClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _ImageClassifier)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name='')
      options = _ImageClassifierOptions(base_options=base_options)
      _ImageClassifier.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, 'rb') as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _ImageClassifierOptions(base_options=base_options)
      classifier = _ImageClassifier.create_from_options(options)
      self.assertIsInstance(classifier, _ImageClassifier)

  @parameterized.parameters(
      (ModelFileType.FILE_NAME, 3, _EXPECTED_CLASSIFICATION_RESULT),
      (ModelFileType.FILE_CONTENT, 3, _EXPECTED_CLASSIFICATION_RESULT))
  def test_classify_model(self, model_file_type, max_results,
                          expected_classification_result):
    # Creates classifier.
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=self.model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(self.model_path, 'rb') as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError('model_file_type is invalid.')

    classifier = _create_classifier_from_options(
        base_options, max_results=max_results)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)

    # Comparing results (classification w/o bounding box).
    self.assertProtoEquals(image_result.to_pb2(),
                           expected_classification_result.to_pb2())

  def test_classify_model_with_bounding_box(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(base_options, max_results=3)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Bounding box in "burger.jpg" corresponding to "burger_crop.jpg".
    bounding_box = bounding_box_pb2.BoundingBox(
        origin_x=0, origin_y=0, width=400, height=325)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box)

    # Expected results.
    expected_classification_result = _ClassificationResult(classifications=[
        _Classifications(
            categories=[
                _Category(
                    index=934,
                    score=0.881507,
                    display_name='',
                    category_name='cheeseburger'),
                _Category(
                    index=925,
                    score=0.019457,
                    display_name='',
                    category_name='guacamole'),
                _Category(
                    index=932,
                    score=0.012489,
                    display_name='',
                    category_name='bagel')
            ],
            head_index=0,
            head_name='')
    ])

    # Comparing results (classification w/ bounding box).
    self.assertProtoEquals(image_result.to_pb2(),
                           expected_classification_result.to_pb2())

  def test_max_results_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, max_results=_MAX_RESULTS)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    categories = image_result.classifications[0].categories

    self.assertLessEqual(
        len(categories), _MAX_RESULTS, 'Too many results returned.')

  def test_score_threshold_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, score_threshold=_SCORE_THRESHOLD)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    categories = image_result.classifications[0].categories

    for category in categories:
      self.assertGreaterEqual(
          category.score, _SCORE_THRESHOLD,
          f'Classification with score lower than threshold found. {category}')

  def test_allowlist_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, category_name_allowlist=_ALLOW_LIST)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    categories = image_result.classifications[0].categories

    for category in categories:
      label = category.category_name
      self.assertIn(label, _ALLOW_LIST,
                    f'Label {label} found but not in label allow list')

  def test_denylist_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, score_threshold=0.01, category_name_denylist=_DENY_LIST)

    # Loads image
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    categories = image_result.classifications[0].categories

    for category in categories:
      label = category.category_name
      self.assertNotIn(label, _DENY_LIST,
                       f'Label {label} found but in deny list.')

  def test_combined_allowlist_and_denylist(self):
    # Fails with combined allowlist and denylist
    with self.assertRaisesRegex(
        ValueError,
        r'`class_name_whitelist` and `class_name_blacklist` are mutually '
        r'exclusive options.'):
      base_options = _BaseOptions(file_name=self.model_path)
      classification_options = classification_options_pb2.ClassificationOptions(
          category_name_allowlist=['foo'], category_name_denylist=['bar'])
      options = _ImageClassifierOptions(
          base_options=base_options,
          classification_options=classification_options)
      _ImageClassifier.create_from_options(options)


if __name__ == '__main__':
  tf.test.main()
