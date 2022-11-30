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
# ==============================================================================
"""Tests for wrtier util methods."""

import array
import tensorflow as tf

from tensorflow_lite_support.metadata import schema_py_generated as _schema_fb
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.metadata_writers import writer_utils
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_FLOAT_TYPE = _schema_fb.TensorType.FLOAT32
_UINT8_TYPE = _schema_fb.TensorType.UINT8
# mobilebert_float.tflite has 1 input tensor and 4 output tensors.
_MODEL_NAME = "../testdata/object_detector/ssd_mobilenet_v1.tflite"
_IMAGE_TENSOR_INDEX = 0
_EXPECTED_INPUT_TYPES = _UINT8_TYPE
_EXPECTED_INPUT_IMAGE_SHAPE = (1, 300, 300, 3)
_EXPECTED_OUTPUT_TYPES = (_FLOAT_TYPE, _FLOAT_TYPE, _FLOAT_TYPE, _FLOAT_TYPE)
_EXOECTED_INPUT_TENSOR_NAMES = "normalized_input_image_tensor"
_EXOECTED_OUTPUT_TENSOR_NAMES = ("TFLite_Detection_PostProcess",
                                 "TFLite_Detection_PostProcess:1",
                                 "TFLite_Detection_PostProcess:2",
                                 "TFLite_Detection_PostProcess:3")


class WriterUtilsTest(tf.test.TestCase):

  def test_compute_flat_size(self):
    shape = array.array("i", [1, 2, 3])
    expected_flat_size = 6

    flat_size = writer_utils.compute_flat_size(shape)
    self.assertEqual(flat_size, expected_flat_size)

  def test_compute_flat_size_with_none_shape(self):
    shape = None
    expected_flat_size = 0

    flat_size = writer_utils.compute_flat_size(shape)
    self.assertEqual(flat_size, expected_flat_size)

  def test_get_input_tensor_names(self):
    tensor_names = writer_utils.get_input_tensor_names(
        model_buffer=test_utils.load_file(_MODEL_NAME))
    self.assertEqual(tensor_names, [_EXOECTED_INPUT_TENSOR_NAMES])

  def test_get_output_tensor_names(self):
    tensor_names = writer_utils.get_output_tensor_names(
        model_buffer=test_utils.load_file(_MODEL_NAME))
    self.assertEqual(tensor_names, list(_EXOECTED_OUTPUT_TENSOR_NAMES))

  def test_get_input_tensor_types(self):
    tensor_types = writer_utils.get_input_tensor_types(
        model_buffer=test_utils.load_file(_MODEL_NAME))
    self.assertEqual(tensor_types, [_EXPECTED_INPUT_TYPES])

  def test_get_output_tensor_types(self):
    tensor_types = writer_utils.get_output_tensor_types(
        model_buffer=test_utils.load_file(_MODEL_NAME))
    self.assertEqual(tensor_types, list(_EXPECTED_OUTPUT_TYPES))

  def test_get_input_tensor_shape(self):
    tensor_shape = writer_utils.get_input_tensor_shape(
        test_utils.load_file(_MODEL_NAME), _IMAGE_TENSOR_INDEX)
    self.assertEqual(list(tensor_shape), list(_EXPECTED_INPUT_IMAGE_SHAPE))

  def test_save_and_load_file(self):
    expected_file_bytes = b"This is a test file."
    file_path = self.create_tempfile().full_path

    writer_utils.save_file(expected_file_bytes, file_path)
    file_bytes = writer_utils.load_file(file_path)
    self.assertEqual(file_bytes, expected_file_bytes)

  def test_get_tokenizer_associated_files_with_bert_tokenizer(self):
    # Create Bert tokenizer
    vocab_file = "vocab.txt"
    tokenizer_md = metadata_info.BertTokenizerMd(vocab_file)

    associated_files = writer_utils.get_tokenizer_associated_files(
        tokenizer_md.create_metadata().options)
    self.assertEqual(associated_files, [vocab_file])

  def test_get_tokenizer_associated_files_with_sentence_piece_tokenizer(self):
    # Create Sentence Piece tokenizer
    vocab_file = "vocab.txt"
    sp_model = "sp.model"
    tokenizer_md = metadata_info.SentencePieceTokenizerMd(sp_model, vocab_file)

    associated_files = writer_utils.get_tokenizer_associated_files(
        tokenizer_md.create_metadata().options)
    self.assertEqual(set(associated_files), set([vocab_file, sp_model]))

  def test_get_tokenizer_associated_files_with_regex_tokenizer(self):
    # Create Regex tokenizer
    delim_regex_pattern = r"[^\w\']+"
    vocab_file = "vocab.txt"
    tokenizer_md = metadata_info.RegexTokenizerMd(delim_regex_pattern,
                                                  vocab_file)

    associated_files = writer_utils.get_tokenizer_associated_files(
        tokenizer_md.create_metadata().options)
    self.assertEqual(associated_files, [vocab_file])


if __name__ == "__main__":
  tf.test.main()
