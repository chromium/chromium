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
"""Tests for bert_nl_classifier."""

import enum

from absl.testing import parameterized

import tensorflow as tf
from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import class_pb2
from tensorflow_lite_support.python.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.text import bert_nl_classifier
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_BertNLClassifier = bert_nl_classifier.BertNLClassifier
_Category = class_pb2.Category
_Classifications = classifications_pb2.Classifications
_ClassificationResult = classifications_pb2.ClassificationResult
_BertNLClassifierOptions = bert_nl_classifier.BertNLClassifierOptions

_BERT_MODEL = 'bert_nl_classifier.tflite'

_POSITIVE_INPUT = "it's a charming and often affecting journey"
_EXPECTED_RESULTS_OF_POSITIVE_INPUT = _ClassificationResult(classifications=[
    _Classifications(
        categories=[
            _Category(
                index=0,
                score=5.534091e-05,
                display_name='',
                category_name='negative'),
            _Category(
                index=0,
                score=0.999945,
                display_name='',
                category_name='positive')
        ],
        head_index=0,
        head_name='')
])

_NEGATIVE_INPUT = 'unflinchingly bleak and desperate'
_EXPECTED_RESULTS_OF_NEGATIVE_INPUT = _ClassificationResult(classifications=[
    _Classifications(
        categories=[
            _Category(
                index=0,
                score=0.956317,
                display_name='',
                category_name='negative'),
            _Category(
                index=0,
                score=0.043683,
                display_name='',
                category_name='positive')
        ],
        head_index=0,
        head_name='')
])


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class BertNLClassifierTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.model_path = test_util.get_test_data_path(_BERT_MODEL)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    classifier = _BertNLClassifier.create_from_file(self.model_path)
    self.assertIsInstance(classifier, _BertNLClassifier)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _BertNLClassifierOptions(base_options=base_options)
    classifier = _BertNLClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _BertNLClassifier)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name='')
      options = _BertNLClassifierOptions(base_options=base_options)
      _BertNLClassifier.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, 'rb') as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _BertNLClassifierOptions(base_options=base_options)
      classifier = _BertNLClassifier.create_from_options(options)
      self.assertIsInstance(classifier, _BertNLClassifier)

  @parameterized.parameters(
      (_BERT_MODEL, ModelFileType.FILE_NAME, _POSITIVE_INPUT,
       _EXPECTED_RESULTS_OF_POSITIVE_INPUT),
      (_BERT_MODEL, ModelFileType.FILE_CONTENT, _POSITIVE_INPUT,
       _EXPECTED_RESULTS_OF_POSITIVE_INPUT),
      (_BERT_MODEL, ModelFileType.FILE_NAME, _NEGATIVE_INPUT,
       _EXPECTED_RESULTS_OF_NEGATIVE_INPUT),
      (_BERT_MODEL, ModelFileType.FILE_CONTENT, _NEGATIVE_INPUT,
       _EXPECTED_RESULTS_OF_NEGATIVE_INPUT))
  def test_classify_model(self, model_name, model_file_type, text,
                          expected_classification_result):
    # Creates classifier.
    model_path = test_util.get_test_data_path(model_name)
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(model_path, 'rb') as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError('model_file_type is invalid.')

    options = _BertNLClassifierOptions(base_options=base_options)
    classifier = _BertNLClassifier.create_from_options(options)
    # Classifies text using the given model.
    text_classification_result = classifier.classify(text)
    self.assertProtoEquals(text_classification_result.to_pb2(),
                           expected_classification_result.to_pb2())


if __name__ == '__main__':
  tf.test.main()
