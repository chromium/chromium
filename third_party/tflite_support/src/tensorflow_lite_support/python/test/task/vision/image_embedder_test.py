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
"""Tests for image_embedder."""

import enum

from absl.testing import parameterized
import numpy as np
import tensorflow as tf

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_pb2
from tensorflow_lite_support.python.task.vision import image_embedder
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.test import test_util


_BaseOptions = base_options_module.BaseOptions
_ImageEmbedder = image_embedder.ImageEmbedder
_ImageEmbedderOptions = image_embedder.ImageEmbedderOptions


class ModelFileType(enum.Enum):
  FILE_CONTENT = 1
  FILE_NAME = 2


class ImageEmbedderTest(parameterized.TestCase, tf.test.TestCase):

  def setUp(self):
    super().setUp()
    self.model_path = test_util.get_test_data_path(
        "mobilenet_v3_small_100_224_embedder.tflite")

  def test_create_from_file_succeeds_with_valid_model_path(self):
    # Creates with default option and valid model file successfully.
    embedder = _ImageEmbedder.create_from_file(self.model_path)
    self.assertIsInstance(embedder, _ImageEmbedder)

  def test_create_from_options_succeeds_with_valid_model_path(self):
    # Creates with options containing model file successfully.
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ImageEmbedderOptions(base_options=base_options)
    embedder = _ImageEmbedder.create_from_options(options)
    self.assertIsInstance(embedder, _ImageEmbedder)

  def test_create_from_options_fails_with_invalid_model_path(self):
    # Invalid empty model path.
    with self.assertRaisesRegex(
        ValueError,
        r"ExternalFile must specify at least one of 'file_content', "
        r"'file_name' or 'file_descriptor_meta'."):
      base_options = _BaseOptions(file_name="")
      options = _ImageEmbedderOptions(base_options=base_options)
      _ImageEmbedder.create_from_options(options)

  def test_create_from_options_succeeds_with_valid_model_content(self):
    # Creates with options containing model content successfully.
    with open(self.model_path, "rb") as f:
      base_options = _BaseOptions(file_content=f.read())
      options = _ImageEmbedderOptions(base_options=base_options)
      embedder = _ImageEmbedder.create_from_options(options)
      self.assertIsInstance(embedder, _ImageEmbedder)

  @parameterized.parameters(
      (False, False, False, ModelFileType.FILE_NAME, 0.932738, -0.20580328),
      (True, False, False, ModelFileType.FILE_NAME, 0.932738, -0.0135661615),
      (True, True, False, ModelFileType.FILE_CONTENT, 0.929717, 254),
      (False, False, True, ModelFileType.FILE_CONTENT, 0.999914, -0.16619979),
  )
  def test_embed(self, l2_normalize, quantize, with_bounding_box,
                 model_file_type, expected_similarity, expected_first_value):
    # Creates embedder.
    if model_file_type is ModelFileType.FILE_NAME:
      base_options = _BaseOptions(file_name=self.model_path)
    elif model_file_type is ModelFileType.FILE_CONTENT:
      with open(self.model_path, "rb") as f:
        model_content = f.read()
      base_options = _BaseOptions(file_content=model_content)
    else:
      # Should never happen
      raise ValueError("model_file_type is invalid.")

    embedding_options = embedding_options_pb2.EmbeddingOptions(
        l2_normalize=l2_normalize, quantize=quantize)
    options = _ImageEmbedderOptions(
        base_options=base_options, embedding_options=embedding_options)
    embedder = _ImageEmbedder.create_from_options(options)

    # Loads images: one is a crop of the other.
    image = tensor_image.TensorImage.create_from_file(
        test_util.get_test_data_path("burger.jpg"))
    cropped_image = tensor_image.TensorImage.create_from_file(
        test_util.get_test_data_path("burger_crop.jpg"))

    bounding_box = None
    if with_bounding_box:
      # Bounding box in "burger.jpg" corresponding to "burger_crop.jpg".
      bounding_box = bounding_box_pb2.BoundingBox(
          origin_x=0, origin_y=0, width=400, height=325)

    # Extracts both embeddings.
    image_result = embedder.embed(image, bounding_box)
    crop_result = embedder.embed(cropped_image)

    # Checks results sizes.
    self.assertLen(image_result.embeddings, 1)
    image_feature_vector = image_result.embeddings[0].feature_vector
    self.assertLen(crop_result.embeddings, 1)
    crop_feature_vector = crop_result.embeddings[0].feature_vector

    self.assertLen(image_feature_vector.value, 1024)
    self.assertLen(crop_feature_vector.value, 1024)

    if quantize:
      self.assertEqual(image_feature_vector.value.dtype, np.uint8)
    else:
      self.assertEqual(image_feature_vector.value.dtype, float)

    # Check embedding value.
    self.assertAlmostEqual(image_feature_vector.value[0], expected_first_value)

    # Checks cosine similarity.
    similarity = embedder.cosine_similarity(image_feature_vector,
                                            crop_feature_vector)
    self.assertAlmostEqual(similarity, expected_similarity, places=6)

  def test_get_embedding_by_index(self):
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ImageEmbedderOptions(base_options=base_options)
    embedder = _ImageEmbedder.create_from_options(options)

    # Builds test data.
    feature_vector = embedding_pb2.FeatureVector(value=np.array([1.0, 0.0]))
    embedding = embedding_pb2.Embedding(
        output_index=0, feature_vector=feature_vector)
    embedding_result = embedding_pb2.EmbeddingResult(embeddings=[embedding])

    result0 = embedder.get_embedding_by_index(embedding_result, 0)
    self.assertEqual(result0.output_index, 0)
    self.assertEqual(result0.feature_vector.value[0], 1.0)
    self.assertEqual(result0.feature_vector.value[1], 0.0)

    with self.assertRaisesRegex(ValueError, r"Output index is out of bound\."):
      embedder.get_embedding_by_index(embedding_result, 1)

  def test_get_embedding_dimension(self):
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ImageEmbedderOptions(base_options=base_options)
    embedder = _ImageEmbedder.create_from_options(options)
    self.assertEqual(embedder.get_embedding_dimension(0), 1024)
    self.assertEqual(embedder.get_embedding_dimension(1), -1)

  def test_number_of_output_layers(self):
    base_options = _BaseOptions(file_name=self.model_path)
    options = _ImageEmbedderOptions(base_options=base_options)
    embedder = _ImageEmbedder.create_from_options(options)
    self.assertEqual(embedder.number_of_output_layers, 1)


if __name__ == "__main__":
  tf.test.main()
