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
"""Tests for audio_classifier."""

import enum

from absl.testing import parameterized
import tensorflow as tf

import unittest
from tensorflow_lite_support.python.task.audio import audio_classifier
from tensorflow_lite_support.python.task.audio.core import audio_record
from tensorflow_lite_support.python.task.audio.core import tensor_audio
from tensorflow_lite_support.python.task.core.proto import base_options_pb2
from tensorflow_lite_support.python.task.processor.proto import classification_options_pb2
from tensorflow_lite_support.python.test import test_util

_mock = unittest.mock
_BaseOptions = base_options_pb2.BaseOptions
_AudioClassifier = audio_classifier.AudioClassifier
_AudioClassifierOptions = audio_classifier.AudioClassifierOptions

_FIXED_INPUT_SIZE_MODEL_FILE = 'yamnet_audio_classifier_with_metadata.tflite'
_SPEECH_AUDIO_FILE = 'speech.wav'
_FIXED_INPUT_SIZE_MODEL_CLASSIFICATIONS = """
classifications {
  classes {
    index: 0
    score: 0.917969
    display_name: ""
    class_name: "Speech"
  }
  classes {
    index: 500
    score: 0.058594
    display_name: ""
    class_name: "Inside, small room"
  }
  classes {
    index: 494
    score: 0.011719
    display_name: ""
    class_name: "Silence"
  }
  head_index: 0
  head_name: "scores"
}
"""

_MULTIHEAD_MODEL_FILE = 'two_heads.tflite'
_TWO_HEADS_AUDIO_FILE = 'two_heads.wav'
_MULTIHEAD_MODEL_CLASSIFICATIONS = """
classifications {
  classes {
    index: 508
    score: 0.548616
    display_name: ""
    class_name: "Environmental noise"
  }
  classes {
    index: 507
    score: 0.380869
    display_name: ""
    class_name: "Noise"
  }
  classes {
    index: 106
    score: 0.256137
    display_name: ""
    class_name: "Bird"
  }
  head_index: 0
  head_name: "yamnet_classification"
}
classifications {
  classes {
    index: 4
    score: 0.933997
    display_name: ""
    class_name: "Chestnut-crowned Antpitta"
  }
  classes {
    index: 1
    score: 0.065934
    display_name: ""
    class_name: "White-breasted Wood-Wren"
  }
  classes {
    index: 0
    score: 6.1469495e-05
    display_name: ""
    class_name: "Red Crossbill"
  }
  head_index: 1
  head_name: "bird_classification"
}
"""

_ALLOW_LIST = ['Speech', 'Inside, small room']
_DENY_LIST = ['Speech']
_SCORE_THRESHOLD = 0.5
_MAX_RESULTS = 3


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


def _create_classifier_from_options(base_options, **classification_options):
  classification_options = classification_options_pb2.ClassificationOptions(
      **classification_options)
  options = _AudioClassifierOptions(
      base_options=base_options, classification_options=classification_options)
  classifier = _AudioClassifier.create_from_options(options)
  return classifier


class AudioClassifierTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.test_audio_path = test_util.get_test_data_path(_SPEECH_AUDIO_FILE)
    self.model_path = test_util.get_test_data_path(_FIXED_INPUT_SIZE_MODEL_FILE)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    classifier = _AudioClassifier.create_from_file(self.model_path)
    self.assertIsInstance(classifier, _AudioClassifier)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _AudioClassifierOptions(base_options=base_options)
    classifier = _AudioClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _AudioClassifier)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name='')
      options = _AudioClassifierOptions(base_options=base_options)
      _AudioClassifier.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, 'rb') as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _AudioClassifierOptions(base_options=base_options)
      classifier = _AudioClassifier.create_from_options(options)
      self.assertIsInstance(classifier, _AudioClassifier)

  def test_create_input_tensor_audio_from_classifier_succeeds(self):
    # Creates TensorAudio instance using the classifier successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _AudioClassifierOptions(base_options=base_options)
    classifier = _AudioClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _AudioClassifier)
    tensor = classifier.create_input_tensor_audio()
    self.assertIsInstance(tensor, tensor_audio.TensorAudio)
    self.assertEqual(tensor.format.channels, 1)
    self.assertEqual(tensor.format.sample_rate, 16000)
    self.assertEqual(tensor.buffer_size, 15600)

  @_mock.patch('sounddevice.InputStream', return_value=_mock.MagicMock())
  def test_create_audio_record_from_classifier_succeeds(self, _):
    # Creates AudioRecord instance using the classifier successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _AudioClassifierOptions(base_options=base_options)
    classifier = _AudioClassifier.create_from_options(options)
    self.assertIsInstance(classifier, _AudioClassifier)
    record = classifier.create_audio_record()
    self.assertIsInstance(record, audio_record.AudioRecord)
    self.assertEqual(record.channels, 1)
    self.assertEqual(record.sampling_rate, 16000)
    self.assertEqual(record.buffer_size, 15600)

  @parameterized.parameters(
      (_FIXED_INPUT_SIZE_MODEL_FILE, ModelFileType.FILE_NAME,
       _SPEECH_AUDIO_FILE, 3, _FIXED_INPUT_SIZE_MODEL_CLASSIFICATIONS),
      (_FIXED_INPUT_SIZE_MODEL_FILE, ModelFileType.FILE_CONTENT,
       _SPEECH_AUDIO_FILE, 3, _FIXED_INPUT_SIZE_MODEL_CLASSIFICATIONS),
      (_MULTIHEAD_MODEL_FILE, ModelFileType.FILE_NAME, _TWO_HEADS_AUDIO_FILE, 3,
       _MULTIHEAD_MODEL_CLASSIFICATIONS),
      (_MULTIHEAD_MODEL_FILE, ModelFileType.FILE_CONTENT, _TWO_HEADS_AUDIO_FILE,
       3, _MULTIHEAD_MODEL_CLASSIFICATIONS))
  def test_classify_model(self, model_name, model_file_type, audio_file_name,
                          max_results, expected_result_text_proto):
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

    classifier = _create_classifier_from_options(
        base_options, max_results=max_results)

    # Load the input audio file.
    test_audio_path = test_util.get_test_data_path(audio_file_name)
    tensor = tensor_audio.TensorAudio.create_from_wav_file(
        test_audio_path, classifier.required_input_buffer_size)

    # Classifies the input.
    audio_result = classifier.classify(tensor)

    # Comparing results.
    self.assertProtoEquals(expected_result_text_proto, audio_result.to_pb2())

  def test_max_results_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, max_results=_MAX_RESULTS)

    # Load the input audio file.
    tensor = tensor_audio.TensorAudio.create_from_wav_file(
        self.test_audio_path, classifier.required_input_buffer_size)

    # Classifies the input.
    audio_result = classifier.classify(tensor)
    categories = audio_result.classifications[0].categories

    self.assertLessEqual(
        len(categories), _MAX_RESULTS, 'Too many results returned.')

  def test_score_threshold_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, score_threshold=_SCORE_THRESHOLD)

    # Load the input audio file.
    tensor = tensor_audio.TensorAudio.create_from_wav_file(
        self.test_audio_path, classifier.required_input_buffer_size)

    # Classifies the input.
    audio_result = classifier.classify(tensor)
    categories = audio_result.classifications[0].categories

    for category in categories:
      self.assertGreaterEqual(
          category.score, _SCORE_THRESHOLD,
          'Classification with score lower than threshold found. {0}'.format(
              category))

  def test_allowlist_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, category_name_allowlist=_ALLOW_LIST)

    # Load the input audio file.
    tensor = tensor_audio.TensorAudio.create_from_wav_file(
        self.test_audio_path, classifier.required_input_buffer_size)

    # Classifies the input.
    audio_result = classifier.classify(tensor)
    categories = audio_result.classifications[0].categories

    for category in categories:
      label = category.category_name
      self.assertIn(
          label, _ALLOW_LIST,
          'Label "{0}" found but not in label allow list'.format(label))

  def test_denylist_option(self):
    # Creates classifier.
    base_options = _BaseOptions(file_name=self.model_path)

    classifier = _create_classifier_from_options(
        base_options, score_threshold=0.01, category_name_denylist=_DENY_LIST)

    # Load the input audio file.
    tensor = tensor_audio.TensorAudio.create_from_wav_file(
        self.test_audio_path, classifier.required_input_buffer_size)

    # Classifies the input.
    audio_result = classifier.classify(tensor)
    categories = audio_result.classifications[0].categories

    for category in categories:
      label = category.category_name
      self.assertNotIn(label, _DENY_LIST,
                       'Label "{0}" found but in deny list.'.format(label))

  def test_combined_allowlist_and_denylist(self):
    # Fails with combined allowlist and denylist
    with self.assertRaisesRegex(
        ValueError,
        r'`class_name_allowlist` and `class_name_denylist` are mutually '
        r'exclusive options.'):
      base_options = _BaseOptions(file_name=self.model_path)
      classification_options = classification_options_pb2.ClassificationOptions(
          category_name_allowlist=['foo'], category_name_denylist=['bar'])
      options = _AudioClassifierOptions(
          base_options=base_options,
          classification_options=classification_options)
      _AudioClassifier.create_from_options(options)


if __name__ == '__main__':
  tf.test.main()
