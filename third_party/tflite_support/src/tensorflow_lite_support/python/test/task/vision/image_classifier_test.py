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
import json

from absl.testing import parameterized
from google.protobuf import json_format
# TODO(b/220067158): Change to import tensorflow and leverage tf.test once
# fixed the dependency issue.
import unittest
from tensorflow_lite_support.python.task.core import task_options
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import class_pb2
from tensorflow_lite_support.python.task.processor.proto import classification_options_pb2
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.vision import image_classifier
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import base_test
from tensorflow_lite_support.python.test import test_util

_BaseOptions = task_options.BaseOptions
_ExternalFile = task_options.ExternalFile
_ImageClassifier = image_classifier.ImageClassifier
_ImageClassifierOptions = image_classifier.ImageClassifierOptions

_MODEL_FILE = 'mobilenet_v2_1.0_224.tflite'
_IMAGE_FILE = 'burger.jpg'
_ALLOW_LIST = ['cheeseburger', 'guacamole']
_DENY_LIST = ['cheeseburger']
_SCORE_THRESHOLD = 0.5
_MAX_RESULTS = 3
_ACCEPTABLE_ERROR_RANGE = 0.000001


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class ImageClassifierTest(parameterized.TestCase, base_test.BaseTestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.model_path = test_util.get_test_data_path(_MODEL_FILE)

  @staticmethod
  def create_classifier_from_options(model_file, **classification_options):
    base_options = _BaseOptions(model_file=model_file)
    classification_options = classification_options_pb2.ClassificationOptions(
        **classification_options)
    options = _ImageClassifierOptions(
        base_options=base_options,
        classification_options=classification_options)
    classifier = _ImageClassifier.create_from_options(options)
    return classifier

  @staticmethod
  def build_test_data(expected_categories):
    classifications = classifications_pb2.Classifications(head_index=0)
    classifications.classes.extend(
        [class_pb2.Category(**args) for args in expected_categories])
    expected_result = classifications_pb2.ClassificationResult()
    expected_result.classifications.append(classifications)
    expected_result_dict = json.loads(
        json_format.MessageToJson(expected_result))

    return expected_result_dict

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(
        model_file=_ExternalFile(file_name=self.model_path))
    options = _ImageClassifierOptions(base_options=base_options)
    classifier = _ImageClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _ImageClassifier)

  def test_create_from_options_fails_with_missing_model_file(self):
    # Missing the model file.
    with self.assertRaisesRegex(
        TypeError,
        r"__init__\(\) missing 1 required positional argument: 'model_file'"):
      _BaseOptions()

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        Exception,
        r'INVALID_ARGUMENT: Expected exactly one of `base_options.model_file` '
        r'or `model_file_with_metadata` to be provided, found 0. '
        r"\[tflite::support::TfLiteSupportStatus='2']"):
      base_options = _BaseOptions(model_file=_ExternalFile(file_name=''))
      options = _ImageClassifierOptions(base_options=base_options)
      _ImageClassifier.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, 'rb') as f:
      base_options = _BaseOptions(
          model_file=_ExternalFile(file_content=f.read()))
      options = _ImageClassifierOptions(base_options=base_options)
      classifier = _ImageClassifier.create_from_options(options)
      self.assertIsInstance(classifier, _ImageClassifier)

  @parameterized.parameters((ModelFileType.FILE_NAME, 3, [{
      'index': 934,
      'score': 0.7399742007255554,
      'class_name': 'cheeseburger'
  }, {
      'index': 925,
      'score': 0.026928534731268883,
      'class_name': 'guacamole'
  }, {
      'index': 932,
      'score': 0.025737214833498,
      'class_name': 'bagel'
  }]), (ModelFileType.FILE_CONTENT, 3, [{
      'index': 934,
      'score': 0.7399742007255554,
      'class_name': 'cheeseburger'
  }, {
      'index': 925,
      'score': 0.026928534731268883,
      'class_name': 'guacamole'
  }, {
      'index': 932,
      'score': 0.025737214833498,
      'class_name': 'bagel'
  }]))
  def test_classify_model(self, model_file_type, max_results,
                          expected_categories):
    # Creates classifier.
    if model_file_type is ModelFileType.FILE_NAME:
      model_file = _ExternalFile(file_name=self.model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(self.model_path, 'rb') as f:
        model_content = f.read()
      model_file = _ExternalFile(file_content=model_content)
    else:
      # Should never happen
      raise ValueError('model_file_type is invalid.')

    classifier = self.create_classifier_from_options(
        model_file, max_results=max_results)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    # Builds test data.
    expected_result_dict = self.build_test_data(expected_categories)

    # Comparing results (classification w/o bounding box).
    self.assertDeepAlmostEqual(
        image_result_dict, expected_result_dict, delta=_ACCEPTABLE_ERROR_RANGE)

  def test_classify_model_with_bounding_box(self):
    # Creates classifier.
    model_file = _ExternalFile(file_name=self.model_path)

    classifier = self.create_classifier_from_options(model_file, max_results=3)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Bounding box in "burger.jpg" corresponding to "burger_crop.jpg".
    bounding_box = bounding_box_pb2.BoundingBox(
        origin_x=0, origin_y=0, width=400, height=325)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    # Expected results.
    expected_categories = [{
        'index': 934,
        'score': 0.8815076351165771,
        'class_name': 'cheeseburger'
    }, {
        'index': 925,
        'score': 0.019456762820482254,
        'class_name': 'guacamole'
    }, {
        'index': 932,
        'score': 0.012489477172493935,
        'class_name': 'bagel'
    }]

    # Builds test data.
    expected_result_dict = self.build_test_data(expected_categories)

    # Comparing results (classification w/ bounding box).
    self.assertDeepAlmostEqual(
        image_result_dict, expected_result_dict, delta=_ACCEPTABLE_ERROR_RANGE)

  def test_max_results_option(self):
    # Creates classifier.
    model_file = _ExternalFile(file_name=self.model_path)

    classifier = self.create_classifier_from_options(
        model_file, max_results=_MAX_RESULTS)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    categories = image_result_dict['classifications'][0]['classes']

    self.assertLessEqual(
        len(categories), _MAX_RESULTS, 'Too many results returned.')

  def test_score_threshold_option(self):
    # Creates classifier.
    model_file = _ExternalFile(file_name=self.model_path)

    classifier = self.create_classifier_from_options(
        model_file, score_threshold=_SCORE_THRESHOLD)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    categories = image_result_dict['classifications'][0]['classes']

    for category in categories:
      score = category['score']
      self.assertGreaterEqual(
          score, _SCORE_THRESHOLD,
          'Classification with score lower than threshold found. {0}'.format(
              category))

  def test_allowlist_option(self):
    # Creates classifier.
    model_file = _ExternalFile(file_name=self.model_path)

    classifier = self.create_classifier_from_options(
        model_file, class_name_allowlist=_ALLOW_LIST)

    # Loads image.
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    categories = image_result_dict['classifications'][0]['classes']

    for category in categories:
      label = category['className']
      self.assertIn(
          label, _ALLOW_LIST,
          'Label "{0}" found but not in label allow list'.format(label))

  def test_denylist_option(self):
    # Creates classifier.
    model_file = _ExternalFile(file_name=self.model_path)

    classifier = self.create_classifier_from_options(
        model_file, score_threshold=0.01, class_name_denylist=_DENY_LIST)

    # Loads image
    image = tensor_image.TensorImage.from_file(self.test_image_path)

    # Classifies the input.
    image_result = classifier.classify(image, bounding_box=None)
    image_result_dict = json.loads(json_format.MessageToJson(image_result))

    categories = image_result_dict['classifications'][0]['classes']

    for category in categories:
      label = category['className']
      self.assertNotIn(label, _DENY_LIST,
                       'Label "{0}" found but in deny list.'.format(label))

  def test_combined_allowlist_and_denylist(self):
    # Fails with combined allowlist and denylist
    with self.assertRaisesRegex(
        Exception,
        r'INVALID_ARGUMENT: `class_name_whitelist` and `class_name_blacklist` '
        r'are mutually exclusive options. '
        r"\[tflite::support::TfLiteSupportStatus='2'\]"):
      base_options = _BaseOptions(
          model_file=_ExternalFile(file_name=self.model_path))
      classification_options = classification_options_pb2.ClassificationOptions(
          class_name_allowlist=['foo'], class_name_denylist=['bar'])
      options = _ImageClassifierOptions(
          base_options=base_options,
          classification_options=classification_options)
      _ImageClassifier.create_from_options(options)

  def test_equal(self):
    base_options1 = _BaseOptions(
        model_file=_ExternalFile(file_name=self.model_path))
    options1 = _ImageClassifierOptions(base_options=base_options1)
    classifier1 = _ImageClassifier.create_from_options(options1)
    # Checks the same classifier object.
    self.assertEqual(classifier1, classifier1)

    base_options2 = _BaseOptions(
        model_file=_ExternalFile(file_name=self.model_path))
    options2 = _ImageClassifierOptions(base_options=base_options2)
    classifier2 = _ImageClassifier.create_from_options(options2)
    # Checks the classifiers with same file name.
    self.assertEqual(classifier1, classifier2)

    with open(self.model_path, 'rb') as f:
      model_content = f.read()
    base_options3 = _BaseOptions(
        model_file=_ExternalFile(file_content=model_content))
    options3 = _ImageClassifierOptions(base_options=base_options3)
    classifier3 = _ImageClassifier.create_from_options(options3)
    # Checks one classifier with file_name and the other with model_content.
    self.assertNotEqual(classifier1, classifier3)

    base_options4 = _BaseOptions(
        model_file=_ExternalFile(file_name=self.model_path))
    options4 = _ImageClassifierOptions(base_options=base_options4)
    options4.classification_options = classification_options_pb2.ClassificationOptions(
        score_threshold=0.5)
    classifier4 = _ImageClassifier.create_from_options(options4)
    # Checks the classifiers with different classification options.
    self.assertNotEqual(classifier1, classifier4)


if __name__ == '__main__':
  unittest.main()
