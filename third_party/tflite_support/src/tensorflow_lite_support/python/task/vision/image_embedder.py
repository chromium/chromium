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
"""Image embedder task."""

import dataclasses
from typing import Optional

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_pb2
from tensorflow_lite_support.python.task.vision.core import tensor_image
from tensorflow_lite_support.python.task.vision.core.pybinds import image_utils
from tensorflow_lite_support.python.task.vision.pybinds import _pywrap_image_embedder

_CppImageEmbedder = _pywrap_image_embedder.ImageEmbedder
_BaseOptions = base_options_module.BaseOptions
_EmbeddingOptions = embedding_options_pb2.EmbeddingOptions


@dataclasses.dataclass
class ImageEmbedderOptions:
  """Options for the image embedder task.

  Attributes:
    base_options: Base options for the image embedder task.
    embedding_options: Embedding options for the image embedder task.
  """

  base_options: _BaseOptions
  embedding_options: _EmbeddingOptions = dataclasses.field(
      default_factory=_EmbeddingOptions
  )


class ImageEmbedder(object):
  """Class that performs dense feature vector extraction on images."""

  def __init__(self, options: ImageEmbedderOptions,
               cpp_embedder: _CppImageEmbedder) -> None:
    """Initializes the `ImageEmbedder` object."""
    # Creates the object of C++ ImageEmbedder class.
    self._options = options
    self._embedder = cpp_embedder

  @classmethod
  def create_from_file(cls, file_path: str) -> "ImageEmbedder":
    """Creates the `ImageEmbedder` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `ImageEmbedder` object that's created from the model file.

    Raises:
      ValueError: If failed to create `ImageEmbedder` object from the provided
        file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = ImageEmbedderOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls,
                          options: ImageEmbedderOptions) -> "ImageEmbedder":
    """Creates the `ImageEmbedder` object from image embedder options.

    Args:
      options: Options for the image embedder task.

    Returns:
      `ImageEmbedder` object that's created from `options`.

    Raises:
      ValueError: If failed to create `ImageEmbdder` object from
        `ImageEmbedderOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    embedder = _CppImageEmbedder.create_from_options(
        options.base_options.to_pb2(), options.embedding_options.to_pb2())
    return cls(options, embedder)

  def embed(
      self,
      image: tensor_image.TensorImage,
      bounding_box: Optional[bounding_box_pb2.BoundingBox] = None
  ) -> embedding_pb2.EmbeddingResult:
    """Performs actual feature vector extraction on the provided TensorImage.

    Args:
      image: Tensor image, used to extract the feature vectors.
      bounding_box: Bounding box, optional. If set, performed feature vector
        extraction only on the provided region of interest. Note that the region
        of interest is not clamped, so this method will fail if the region is
        out of bounds of the input image.

    Returns:
      The embedding result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to calculate the embedding vector.
    """
    image_data = image_utils.ImageData(image.buffer)

    if bounding_box is None:
      embedding_result = self._embedder.embed(image_data)
    else:
      embedding_result = self._embedder.embed(image_data, bounding_box.to_pb2())

    return embedding_pb2.EmbeddingResult.create_from_pb2(embedding_result)

  def get_embedding_by_index(self, result: embedding_pb2.EmbeddingResult,
                             output_index: int) -> embedding_pb2.Embedding:
    """Gets the embedding in the embedding result by `output_index`.

    Args:
      result: embedding result.
      output_index: output index of the output layer.

    Returns:
      The Embedding output by the output_index'th layer. In (the most common)
      case where a single embedding is produced, you can just call
      get_feature_vector_by_index(result, 0).

    Raises:
      ValueError if the output index is out of bound.
    """
    if output_index < 0 or output_index >= len(result.embeddings):
      raise ValueError("Output index is out of bound.")
    embedding = self._embedder.get_embedding_by_index(result.to_pb2(),
                                                      output_index)
    return embedding_pb2.Embedding.create_from_pb2(embedding)

  def cosine_similarity(self, u: embedding_pb2.FeatureVector,
                        v: embedding_pb2.FeatureVector) -> float:
    """Computes cosine similarity [1] between two feature vectors."""
    return self._embedder.cosine_similarity(u.to_pb2(), v.to_pb2())

  def get_embedding_dimension(self, output_index: int) -> int:
    """Gets the dimensionality of the embedding output.

    Args:
      output_index: The output index of output layer.

    Returns:
      Dimensionality of the embedding output by the output_index'th output
      layer. Returns -1 if `output_index` is out of bounds.
    """
    return self._embedder.get_embedding_dimension(output_index)

  @property
  def number_of_output_layers(self) -> int:
    """Gets the number of output layers of the model."""
    return self._embedder.get_number_of_output_layers()

  @property
  def options(self) -> ImageEmbedderOptions:
    return self._options
