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
"""Tests for image_searcher."""

import enum

from absl.testing import parameterized

import tensorflow as tf
from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_options_pb2
from tensorflow_lite_support.python.task.processor.proto import search_result_pb2
from tensorflow_lite_support.python.task.vision import image_searcher
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import test_util

_BaseOptions = base_options_module.BaseOptions
_EmbeddingOptions = embedding_options_pb2.EmbeddingOptions
_SearchOptions = search_options_pb2.SearchOptions
_SearchResult = search_result_pb2.SearchResult
_NearestNeighbor = search_result_pb2.NearestNeighbor
_ImageSearcher = image_searcher.ImageSearcher
_ImageSearcherOptions = image_searcher.ImageSearcherOptions

_MOBILENET_EMBEDDER_MODEL = 'mobilenet_v3_small_100_224_embedder.tflite'
_MOBILENET_SEARCHER_MODEL = 'mobilenet_v3_small_100_224_searcher.tflite'
_MOBILENET_INDEX = 'searcher_index.ldb'
_EXPECTED_MOBILENET_DEFAULT_OPTIONS_SEARCH_RESULT = _SearchResult(
    nearest_neighbors=[
        _NearestNeighbor(metadata=bytearray(b'burger'), distance=200.798508),
        _NearestNeighbor(metadata=bytearray(b'car'), distance=228.445480),
        _NearestNeighbor(metadata=bytearray(b'bird'), distance=230.091507),
        _NearestNeighbor(metadata=bytearray(b'dog'), distance=231.857605),
        _NearestNeighbor(metadata=bytearray(b'cat'), distance=232.290115)
    ])

_IMAGE_FILE = 'burger.jpg'
_MAX_RESULTS = 2


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class IndexFileType(enum.Enum):
  NONE = 1
  FILE_CONTENT = 2
  FILE_NAME = 3


class ImageSearcherTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.test_image_path = test_util.get_test_data_path(_IMAGE_FILE)
    self.embedder_model_path = test_util.get_test_data_path(
        _MOBILENET_EMBEDDER_MODEL)
    self.searcher_model_path = test_util.get_test_data_path(
        _MOBILENET_SEARCHER_MODEL)
    self.index_path = test_util.get_test_data_path(_MOBILENET_INDEX)

  def test_create_from_file_succeeds_with_valid_embedder_and_index_paths(self):
    # Creates with default option and valid model and index files successfully.
    searcher = _ImageSearcher.create_from_file(self.embedder_model_path,
                                               self.index_path)
    self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_file_succeeds_with_valid_searcher_path(self):
    # Creates with default option and valid searcher model.
    searcher = _ImageSearcher.create_from_file(self.searcher_model_path)
    self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_succeeds_with_valid_embedder_and_index_paths(
      self):
    options = _ImageSearcherOptions(
        base_options=_BaseOptions(file_name=self.embedder_model_path),
        search_options=_SearchOptions(index_file_name=self.index_path))
    searcher = _ImageSearcher.create_from_options(options)
    self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_succeeds_with_valid_searcher_path(self):
    options = _ImageSearcherOptions(
        base_options=_BaseOptions(file_name=self.searcher_model_path),
        search_options=_SearchOptions())
    searcher = _ImageSearcher.create_from_options(options)
    self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_succeeds_with_valid_embedder_content(self):
    # Creates with options containing model content successfully.
    with open(self.embedder_model_path, 'rb') as f:
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_content=f.read()),
          search_options=_SearchOptions(index_file_name=self.index_path))
      searcher = _ImageSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_succeeds_with_valid_searcher_content(self):
    # Creates with options containing model content successfully.
    with open(self.searcher_model_path, 'rb') as f:
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_content=f.read()),
          search_options=_SearchOptions())
      searcher = _ImageSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_succeeds_with_valid_index_content(self):
    # Creates with options containing index content successfully.
    with open(self.index_path, 'rb') as f:
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          search_options=_SearchOptions(index_file_content=f.read()))
      searcher = _ImageSearcher.create_from_options(options)
      self.assertIsInstance(searcher, _ImageSearcher)

  def test_create_from_options_fails_with_invalid_index_path(self):
    # Invalid index path.
    with self.assertRaisesRegex(
        ValueError,
        r'Unable to find index file: SearchOptions.index_file is not set and '
        r'no AssociatedFile with type SCANN_INDEX_FILE could be found in the '
        r'output tensor metadata.'):
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path))
      _ImageSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_name=''),
          search_options=_SearchOptions(index_file_name=self.index_path))
      _ImageSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_quantization(self):
    # Invalid quantization option.
    with self.assertRaisesRegex(
        ValueError,
        r'Setting EmbeddingOptions.quantize = true is not allowed in '
        r'searchers.'):
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          embedding_options=_EmbeddingOptions(quantize=True),
          search_options=_SearchOptions(index_file_name=self.index_path))
      _ImageSearcher.create_from_options(options)

  def test_create_from_options_fails_with_invalid_max_results(self):
    # Invalid max results option.
    with self.assertRaisesRegex(
        ValueError, r'SearchOptions.max_results must be > 0, found -1.'):
      options = _ImageSearcherOptions(
          base_options=_BaseOptions(file_name=self.embedder_model_path),
          search_options=_SearchOptions(
              index_file_name=self.index_path, max_results=-1))
      _ImageSearcher.create_from_options(options)

  def test_search_with_default_options(self):
    # Create searcher.
    searcher = _ImageSearcher.create_from_file(self.embedder_model_path,
                                               self.index_path)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Perform image search.
    image_search_result = searcher.search(image)

    self.assertProtoEquals(
        image_search_result.to_pb2(),
        _EXPECTED_MOBILENET_DEFAULT_OPTIONS_SEARCH_RESULT.to_pb2())

  @parameterized.parameters(
      (_MOBILENET_EMBEDDER_MODEL, ModelFileType.FILE_NAME,
       IndexFileType.FILE_NAME),
      (_MOBILENET_EMBEDDER_MODEL, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_NAME),
      (_MOBILENET_EMBEDDER_MODEL, ModelFileType.FILE_NAME,
       IndexFileType.FILE_CONTENT),
      (_MOBILENET_EMBEDDER_MODEL, ModelFileType.FILE_CONTENT,
       IndexFileType.FILE_CONTENT),
      (_MOBILENET_SEARCHER_MODEL, ModelFileType.FILE_NAME, IndexFileType.NONE),
      (_MOBILENET_SEARCHER_MODEL, ModelFileType.FILE_CONTENT,
       IndexFileType.NONE),
  )
  def test_search(self, model_name, model_file_type, index_file_type):
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
      index_path = test_util.get_test_data_path(_MOBILENET_INDEX)
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
    options = _ImageSearcherOptions(
        base_options, _EmbeddingOptions(l2_normalize=True, quantize=False),
        search_options)
    searcher = _ImageSearcher.create_from_options(options)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Perform image search.
    image_search_result = searcher.search(image)

    # Expected results.
    expected_search_result = _SearchResult(nearest_neighbors=[
        _NearestNeighbor(metadata=bytearray(b'burger'), distance=-0.0),
        _NearestNeighbor(metadata=bytearray(b'car'), distance=1.822435),
        _NearestNeighbor(metadata=bytearray(b'bird'), distance=1.930939),
        _NearestNeighbor(metadata=bytearray(b'dog'), distance=2.047355),
        _NearestNeighbor(metadata=bytearray(b'cat'), distance=2.075868)
    ])

    # Comparing results.
    self.assertProtoEquals(image_search_result.to_pb2(),
                           expected_search_result.to_pb2())

    # Get user info and compare values.
    self.assertEqual(searcher.get_user_info(), 'userinfo')

  def test_search_with_bounding_box(self):
    # Create searcher.
    searcher = _ImageSearcher.create_from_file(self.embedder_model_path,
                                               self.index_path)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Bounding box in "burger.jpg" corresponding to "burger_crop.jpg".
    bounding_box = bounding_box_pb2.BoundingBox(
        origin_x=0, origin_y=0, width=400, height=325)

    # Perform image search.
    image_search_result = searcher.search(image, bounding_box)

    # Expected results.
    expected_search_result = _SearchResult(nearest_neighbors=[
        _NearestNeighbor(metadata=bytearray(b'burger'), distance=184.85214),
        _NearestNeighbor(metadata=bytearray(b'car'), distance=209.32019),
        _NearestNeighbor(metadata=bytearray(b'bird'), distance=211.43195),
        _NearestNeighbor(metadata=bytearray(b'dog'), distance=212.77237),
        _NearestNeighbor(metadata=bytearray(b'cat'), distance=212.8553)
    ])

    # Comparing results.
    self.assertProtoEquals(image_search_result.to_pb2(),
                           expected_search_result.to_pb2())

    # Get user info and compare values.
    self.assertEqual(searcher.get_user_info(), 'userinfo')

  def test_max_results_option(self):
    # Create searcher.
    base_options = _BaseOptions(file_name=self.embedder_model_path)
    search_options = _SearchOptions(
        index_file_name=self.index_path, max_results=_MAX_RESULTS)
    options = _ImageSearcherOptions(base_options,
                                    _EmbeddingOptions(l2_normalize=True),
                                    search_options)
    searcher = _ImageSearcher.create_from_options(options)

    # Loads image.
    image = tensor_image.TensorImage.create_from_file(self.test_image_path)

    # Perform image search.
    image_search_result = searcher.search(image)
    nearest_neighbors = image_search_result.nearest_neighbors

    self.assertLessEqual(
        len(nearest_neighbors), _MAX_RESULTS, 'Too many results returned.')


if __name__ == '__main__':
  tf.test.main()
