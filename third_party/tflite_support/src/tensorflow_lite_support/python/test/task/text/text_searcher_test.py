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
"""Tests for text_searcher."""

import enum

from absl.testing import parameterized

import tensorflow as tf
from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_result_pb2
from tensorflow_lite_support.python.task.text import text_searcher
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_EmbeddingOptions = embedding_options_pb2.EmbeddingOptions
_SearchOptions = search_options_pb2.SearchOptions
_SearchResult = search_result_pb2.SearchResult
_NearestNeighbor = search_result_pb2.NearestNeighbor
_TextSearcher = text_searcher.TextSearcher
_TextSearcherOptions = text_searcher.TextSearcherOptions

_REGEX_EMBEDDER_MODEL = 'regex_one_embedding_with_metadata.tflite'
_REGEX_SEARCHER_MODEL = 'regex_searcher.tflite'
_REGEX_INDEX = 'regex_index.ldb'
_EXPECTED_REGEX_SEARCH_RESULT = _SearchResult(nearest_neighbors=[
    _NearestNeighbor(
        metadata=bytearray(b'The weather was excellent.'), distance=0.0),
    _NearestNeighbor(
        metadata=bytearray(b'The sun was shining on that day.'),
        distance=5.7e-5),
    _NearestNeighbor(
        metadata=bytearray(b'The cat is chasing after the mouse.'),
        distance=8.9e-5),
    _NearestNeighbor(
        metadata=bytearray(b'It was a sunny day.'), distance=0.000113),
    _NearestNeighbor(
        metadata=bytearray(b'He was very happy with his newly bought car.'),
        distance=0.000119)
])

_EXPECTED_REGEX_DEFAULT_OPTIONS_SEARCH_RESULT = _SearchResult(
    nearest_neighbors=[
        _NearestNeighbor(
            metadata=bytearray(b'The weather was excellent.'),
            distance=0.889665),
        _NearestNeighbor(
            metadata=bytearray(b'The sun was shining on that day.'),
            distance=0.889668),
        _NearestNeighbor(
            metadata=bytearray(b'The cat is chasing after the mouse.'),
            distance=0.88967),
        _NearestNeighbor(
            metadata=bytearray(b'It was a sunny day.'), distance=0.889671),
        _NearestNeighbor(
            metadata=bytearray(b'He was very happy with his newly bought car.'),
            distance=0.889672)
    ])

_BERT_EMBEDDER_MODEL = 'mobilebert_embedding_with_metadata.tflite'
_BERT_SEARCHER_MODEL = 'mobilebert_searcher.tflite'
_BERT_INDEX = 'mobilebert_index.ldb'
_EXPECTED_BERT_SEARCH_RESULT = _SearchResult(nearest_neighbors=[
    _NearestNeighbor(
        metadata=bytearray(b'The weather was excellent.'), distance=0.0),
    _NearestNeighbor(
        metadata=bytearray(b'It was a sunny day.'), distance=0.115369),
    _NearestNeighbor(
        metadata=bytearray(b'The sun was shining on that day.'),
        distance=0.230017),
    _NearestNeighbor(
        metadata=bytearray(b'He was very happy with his newly bought car.'),
        distance=0.324563),
    _NearestNeighbor(
        metadata=bytearray(b'The cat is chasing after the mouse.'),
        distance=0.966928)
])

_USE_EMBEDDER_MODEL = 'universal_sentence_encoder_qa_with_metadata.tflite'
_USE_SEARCHER_MODEL = 'universal_sentence_encoder_searcher.tflite'
_USE_INDEX = 'universal_sentence_encoder_index.ldb'
_EXPECTED_USE_SEARCH_RESULT = _SearchResult(nearest_neighbors=[
    _NearestNeighbor(
        metadata=bytearray(b'The weather was excellent.'), distance=0.0),
    _NearestNeighbor(
        metadata=bytearray(b'It was a sunny day.'), distance=0.146359),
    _NearestNeighbor(
        metadata=bytearray(b'The sun was shining on that day.'),
        distance=0.152225),
    _NearestNeighbor(
        metadata=bytearray(b'The cat is chasing after the mouse.'),
        distance=0.359965),
    _NearestNeighbor(
        metadata=bytearray(b'He was very happy with his newly bought car.'),
        distance=0.366927)
])

_MAX_RESULTS = 2


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class IndexFileType(enum.Enum):
  NONE = 1
  FILE_CONTENT = 2
  FILE_NAME = 3


class TextSearcherTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.embedder_model_path = test_util.get_test_data_path(
        _REGEX_EMBEDDER_MODEL)
    self.searcher_model_path = test_util.get_test_data_path(
        _REGEX_SEARCHER_MODEL)
    self.index_path = test_util.get_test_data_path(_REGEX_INDEX)

  def test_create_from_file_succeeds_with_valid_embedder_and_index_paths(self):
    # Creates with default option and valid model and index files successfully.
    searcher = _TextSearcher.create_from_file(self.embedder_model_path,
                                              self.index_path)
    self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_file_succeeds_with_valid_searcher_path(self):
    # Creates with default option and valid model and index files successfully.
    searcher = _TextSearcher.create_from_file(self.searcher_model_path)
    self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_succeeds_with_valid_embedder_and_index_paths(
      self):
    options = _TextSearcherOptions(
        base_options=_BaseOptions(file_name=self.embedder_model_path),
        search_options=_SearchOptions(index_file_name=self.index_path))
    searcher = _TextSearcher.create_from_options(options)
    self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_succeeds_with_valid_searcher_path(self):
    options = _TextSearcherOptions(
        base_options=_BaseOptions(file_name=self.searcher_model_path),
        search_options=_SearchOptions())
    searcher = _TextSearcher.create_from_options(options)
    self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_succeeds_with_valid_embedder_content(self):
    # Creates with options containing model content successfully.
    with open(self.embedder_model_path, 'rb') as f:
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_content=f.read()),
          search_options=_SearchOptions(index_file_name=self.index_path))
      searcher = _TextSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_succeeds_with_valid_searcher_content(self):
    # Creates with options containing model content successfully.
    with open(self.searcher_model_path, 'rb') as f:
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_content=f.read()),
          search_options=_SearchOptions(index_file_name=self.index_path))
      searcher = _TextSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_succeeds_with_valid_index_content(self):
    # Creates with options containing index content successfully.
    with open(self.index_path, 'rb') as f:
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          search_options=_SearchOptions(index_file_content=f.read()))
      searcher = _TextSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _TextSearcher)

  def test_create_from_options_fails_with_invalid_index_path(self):
    # Invalid index path.
    with self.assertRaisesRegex(
        ValueError,
        r'Unable to find index file: SearchOptions.index_file is not set and '
        r'no AssociatedFile with type SCANN_INDEX_FILE could be found in the '
        r'output tensor metadata.'):
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path))
      _TextSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_name=''),
          search_options=_SearchOptions(index_file_name=self.index_path))
      _TextSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_quantization(self):
    # Invalid quantization option.
    with self.assertRaisesRegex(
        ValueError,
        r'Setting EmbeddingOptions.quantize = true is not allowed in '
        r'searchers.'):
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          embedding_options=_EmbeddingOptions(quantize=True),
          search_options=_SearchOptions(index_file_name=self.index_path))
      _TextSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_max_results(self):
    # Invalid max results option.
    with self.assertRaisesRegex(
        ValueError, r'SearchOptions.max_results must be > 0, found -1.'):
      options = _TextSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          search_options=_SearchOptions(
              index_file_name=self.index_path, max_results=-1))
      _TextSearcher.create_from_options(options)

  def test_search_with_default_options(self):
    # Create searcher.
    searcher = _TextSearcher.create_from_file(self.embedder_model_path,
                                              self.index_path)

    # Perform text search.
    text_search_result = searcher.search('The weather was excellent.')

    self.assertProtoEquals(
        text_search_result.to_pb2(),
        _EXPECTED_REGEX_DEFAULT_OPTIONS_SEARCH_RESULT.to_pb2())

  @parameterized.parameters(
      (_REGEX_EMBEDDER_MODEL, _REGEX_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_NAME, _EXPECTED_REGEX_SEARCH_RESULT),
      (_REGEX_EMBEDDER_MODEL, _REGEX_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_NAME, _EXPECTED_REGEX_SEARCH_RESULT),
      (_REGEX_EMBEDDER_MODEL, _REGEX_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_CONTENT, _EXPECTED_REGEX_SEARCH_RESULT),
      (_REGEX_EMBEDDER_MODEL, _REGEX_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_CONTENT, _EXPECTED_REGEX_SEARCH_RESULT),
      (_REGEX_SEARCHER_MODEL, None, ModelFileType.FILE_NAME, IndexFileType.NONE,
       _EXPECTED_REGEX_SEARCH_RESULT),
      (_REGEX_SEARCHER_MODEL, None, ModelFileType.FILE_CONTENT,
       IndexFileType.NONE, _EXPECTED_REGEX_SEARCH_RESULT),
      (_BERT_EMBEDDER_MODEL, _BERT_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_NAME, _EXPECTED_BERT_SEARCH_RESULT),
      (_BERT_EMBEDDER_MODEL, _BERT_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_NAME, _EXPECTED_BERT_SEARCH_RESULT),
      (_BERT_EMBEDDER_MODEL, _BERT_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_CONTENT, _EXPECTED_BERT_SEARCH_RESULT),
      (_BERT_EMBEDDER_MODEL, _BERT_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_CONTENT, _EXPECTED_BERT_SEARCH_RESULT),
      (_BERT_SEARCHER_MODEL, None, ModelFileType.FILE_NAME, IndexFileType.NONE,
       _EXPECTED_BERT_SEARCH_RESULT),
      (_BERT_SEARCHER_MODEL, None, ModelFileType.FILE_CONTENT,
       IndexFileType.NONE, _EXPECTED_BERT_SEARCH_RESULT),
      (_USE_EMBEDDER_MODEL, _USE_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_NAME, _EXPECTED_USE_SEARCH_RESULT),
      (_USE_EMBEDDER_MODEL, _USE_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_NAME, _EXPECTED_USE_SEARCH_RESULT),
      (_USE_EMBEDDER_MODEL, _USE_INDEX, ModelFileType.FILE_NAME,
       IndexFileType.FILE_CONTENT, _EXPECTED_USE_SEARCH_RESULT),
      (_USE_EMBEDDER_MODEL, _USE_INDEX, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_CONTENT, _EXPECTED_USE_SEARCH_RESULT),
      (_USE_SEARCHER_MODEL, None, ModelFileType.FILE_NAME, IndexFileType.NONE,
       _EXPECTED_USE_SEARCH_RESULT),
      (_USE_SEARCHER_MODEL, None, ModelFileType.FILE_CONTENT,
       IndexFileType.NONE, _EXPECTED_USE_SEARCH_RESULT),
  )
  def test_search(self, model_name, index_name, model_file_type,
                  index_file_type, expected_search_result):
    # Create BaseOptions.
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

    # Create SearchOptions.
    if index_file_type is IndexFileType.NONE:
      search_options = _SearchOptions()
    else:
      index_path = test_util.get_test_data_path(index_name)
      if index_file_type is IndexFileType.FILE_NAME:
        search_options = _SearchOptions(index_file_name=index_path)
      elif index_file_type is IndexFileType.FILE_CONTENT:
        with open(index_path, 'rb') as f:
          index_content = f.read()
        search_options = _SearchOptions(index_file_content=index_content)
      else:
        # Should never happen
        raise ValueError('index_file_type is invalid.')

    # Create searcher.
    options = _TextSearcherOptions(
        base_options, _EmbeddingOptions(l2_normalize=True, quantize=False),
        search_options)
    searcher = _TextSearcher.create_from_options(options)

    # Perform text search.
    text_search_result = searcher.search('The weather was excellent.')

    # Comparing results.
    self.assertProtoEquals(text_search_result.to_pb2(),
                           expected_search_result.to_pb2())

    # Get user info and compare values.
    self.assertEqual(searcher.get_user_info(), 'userinfo')

  def test_max_results_option(self):
    # Create searcher.
    base_options = _BaseOptions(file_name=self.embedder_model_path)
    search_options = _SearchOptions(
        index_file_name=self.index_path, max_results=_MAX_RESULTS)
    options = _TextSearcherOptions(base_options,
                                   _EmbeddingOptions(l2_normalize=True),
                                   search_options)
    searcher = _TextSearcher.create_from_options(options)

    # Perform text search.
    text_search_result = searcher.search('The weather was excellent.')
    nearest_neighbors = text_search_result.nearest_neighbors

    self.assertLessEqual(
        len(nearest_neighbors), _MAX_RESULTS, 'Too many results returned.')


if __name__ == '__main__':
  tf.test.main()
