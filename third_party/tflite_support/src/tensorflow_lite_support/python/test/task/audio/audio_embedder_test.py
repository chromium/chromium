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
"""Tests for audio_embedder."""

import enum

from absl.testing import parameterized
# TODO(b/220067158): Change to import tensorflow and leverage tf.test once
# fixed the dependency issue.
import unittest

from tensorflow_lite_support.python.task.audio import audio_embedder
from tensorflow_lite_support.python.task.audio.core import audio_record
from tensorflow_lite_support.python.task.audio.core import tensor_audio
from tensorflow_lite_support.python.task.core.proto import base_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.test import base_test
from tensorflow_lite_support.python.test import test_util

_mock = unittest.mock
_BaseOptions = base_options_pb2.BaseOptions
_AudioEmbedder = audio_embedder.AudioEmbedder
_AudioEmbedderOptions = audio_embedder.AudioEmbedderOptions

_YAMNET_EMBEDDING_MODEL_FILE = "yamnet_embedding_metadata.tflite"


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class AudioEmbedderTest(parameterized.TestCase, base_test.BaseTestCase):

  def setUp(self):
    super().setUp()
    self.model_path = test_util.get_test_data_path(_YAMNET_EMBEDDING_MODEL_FILE)

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    embedder = _AudioEmbedder.create_from_file(self.model_path)
    self.assertIsInstance(embedder, _AudioEmbedder)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    options = _AudioEmbedderOptions(_BaseOptions(file_name=self.model_path))
    embedder = _AudioEmbedder.create_from_options(options)
    self.assertIsInstance(embedder, _AudioEmbedder)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      options = _AudioEmbedderOptions(_BaseOptions(file_name=""))
      _AudioEmbedder.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, "rb") as f:
      options = _AudioEmbedderOptions(_BaseOptions(file_content=f.read()))
      embedder = _AudioEmbedder.create_from_options(options)
      self.assertIsInstance(embedder, _AudioEmbedder)

  def test_create_input_tensor_audio_from_embedder_succeeds(self):
    # Creates TensorAudio instance using the embedder successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _AudioEmbedderOptions(base_options=base_options)
    embedder = _AudioEmbedder.create_from_options(options)
    self.assertIsInstance(embedder, _AudioEmbedder)
    tensor = embedder.create_input_tensor_audio()
    self.assertIsInstance(tensor, tensor_audio.TensorAudio)
    self.assertEqual(tensor.format.channels, 1)
    self.assertEqual(tensor.format.sample_rate, 16000)
    self.assertEqual(tensor.buffer_size, 15600)

  @_mock.patch("sounddevice.InputStream", return_value=_mock.MagicMock())
  def test_create_audio_record_from_embedder_succeeds(self, _):
    # Creates AudioRecord instance using the embedder successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _AudioEmbedderOptions(base_options=base_options)
    embedder = _AudioEmbedder.create_from_options(options)
    self.assertIsInstance(embedder, _AudioEmbedder)
    record = embedder.create_audio_record()
    self.assertIsInstance(record, audio_record.AudioRecord)
    self.assertEqual(record.channels, 1)
    self.assertEqual(record.sampling_rate, 16000)
    self.assertEqual(record.buffer_size, 15600)

  @parameterized.parameters((_YAMNET_EMBEDDING_MODEL_FILE, False, False,
                             ModelFileType.FILE_NAME, 1024, 0.091439),
                            (_YAMNET_EMBEDDING_MODEL_FILE, True, True,
                             ModelFileType.FILE_CONTENT, 1024, 0.092382))
  def test_embed(self, model_name, l2_normalize, quantize, model_file_type,
                 embedding_length, expected_similarity):
    # Create embedder.
    model_path = test_util.get_test_data_path(model_name)
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(model_path, "rb") as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError("model_file_type is invalid.")

    options = _AudioEmbedderOptions(
        base_options,
        embedding_options_pb2.EmbeddingOptions(
            l2_normalize=l2_normalize, quantize=quantize))
    embedder = _AudioEmbedder.create_from_options(options)

    # Load the input audio files.
    tensor0 = tensor_audio.TensorAudio.create_from_wav_file(
        test_util.get_test_data_path("speech.wav"),
        embedder.required_input_buffer_size)

    tensor1 = tensor_audio.TensorAudio.create_from_wav_file(
        test_util.get_test_data_path("two_heads.wav"),
        embedder.required_input_buffer_size)

    # Extract embeddings.
    result0 = embedder.embed(tensor0)
    result1 = embedder.embed(tensor1)

    # Check embedding sizes.
    def _check_embedding_size(result):
      self.assertLen(result.embeddings, 1)
      feature_vector = result.embeddings[0].feature_vector
      if quantize:
        self.assertLen(feature_vector.value_string, embedding_length)
      else:
        self.assertLen(feature_vector.value_float, embedding_length)

    _check_embedding_size(result0)
    _check_embedding_size(result1)

    result0_feature_vector = result0.embeddings[0].feature_vector
    result1_feature_vector = result1.embeddings[0].feature_vector

    if quantize:
      self.assertLen(result0_feature_vector.value_string, 1024)
      self.assertLen(result1_feature_vector.value_string, 1024)
    else:
      self.assertLen(result0_feature_vector.value_float, 1024)
      self.assertLen(result1_feature_vector.value_float, 1024)

    # Checks cosine similarity.
    similarity = embedder.cosine_similarity(result0_feature_vector,
                                            result1_feature_vector)
    self.assertAlmostEqual(similarity, expected_similarity, places=6)

  def test_get_embedding_dimension(self):
    options = _AudioEmbedderOptions(_BaseOptions(file_name=self.model_path))
    embedder = _AudioEmbedder.create_from_options(options)
    self.assertEqual(embedder.get_embedding_dimension(0), 1024)
    self.assertEqual(embedder.get_embedding_dimension(1), -1)

  def test_number_of_output_layers(self):
    options = _AudioEmbedderOptions(_BaseOptions(file_name=self.model_path))
    embedder = _AudioEmbedder.create_from_options(options)
    self.assertEqual(embedder.number_of_output_layers, 1)


if __name__ == "__main__":
  unittest.main()
