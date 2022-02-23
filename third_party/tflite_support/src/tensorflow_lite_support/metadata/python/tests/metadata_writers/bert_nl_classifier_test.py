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
# ==============================================================================
"""Tests for bert_nl_classifier.MetadataWriter."""

import tensorflow as tf

from tensorflow_lite_support.metadata.python import metadata as _metadata
from tensorflow_lite_support.metadata.python.metadata_writers import bert_nl_classifier
from tensorflow_lite_support.metadata.python.metadata_writers import metadata_info
from tensorflow_lite_support.metadata.python.tests.metadata_writers import test_utils

_TEST_DIR = "tensorflow_lite_support/metadata/python/tests/testdata/bert_nl_classifier/"
_MODEL = "../testdata/bert_nl_classifier/bert_nl_classifier_no_metadata.tflite"
_LABEL_FILE = _TEST_DIR + "labels.txt"
_VOCAB_FILE = _TEST_DIR + "mobilebert_vocab.txt"
_SP_MODEL_FILE = _TEST_DIR + "30k-clean.model"
_DELIM_REGEX_PATTERN = r"[^\w\']+"
_JSON_FOR_INFERENCE_WITH_BERT = "../testdata/bert_nl_classifier/bert_nl_classifier_with_bert_tokenizer.json"
_JSON_FOR_INFERENCE_WITH_SENTENCE_PIECE = "../testdata/bert_nl_classifier/bert_nl_classifier_with_sentence_piece.json"
_JSON_DEFAULT = "../testdata/bert_nl_classifier/bert_nl_classifier_default.json"


class MetadataWriterTest(tf.test.TestCase):

  def test_create_for_inference_with_bert_should_succeed(self):
    writer = bert_nl_classifier.MetadataWriter.create_for_inference(
        test_utils.load_file(_MODEL),
        metadata_info.BertTokenizerMd(_VOCAB_FILE), [_LABEL_FILE])

    displayer = _metadata.MetadataDisplayer.with_model_buffer(writer.populate())
    metadata_json = displayer.get_metadata_json()
    expected_json = test_utils.load_file(_JSON_FOR_INFERENCE_WITH_BERT, "r")

    self.assertEqual(metadata_json, expected_json)

  def test_create_for_inference_with_sentence_piece_should_succeed(self):
    writer = bert_nl_classifier.MetadataWriter.create_for_inference(
        test_utils.load_file(_MODEL),
        metadata_info.SentencePieceTokenizerMd(_SP_MODEL_FILE), [_LABEL_FILE])

    displayer = _metadata.MetadataDisplayer.with_model_buffer(writer.populate())
    metadata_json = displayer.get_metadata_json()
    expected_json = test_utils.load_file(
        _JSON_FOR_INFERENCE_WITH_SENTENCE_PIECE, "r")

    self.assertEqual(metadata_json, expected_json)

  def test_create_from_metadata_info_by_default_should_succeed(self):
    writer = bert_nl_classifier.MetadataWriter.create_from_metadata_info(
        test_utils.load_file(_MODEL))

    metadata_json = writer.get_metadata_json()
    expected_json = test_utils.load_file(_JSON_DEFAULT, "r")
    self.assertEqual(metadata_json, expected_json)


if __name__ == "__main__":
  tf.test.main()
