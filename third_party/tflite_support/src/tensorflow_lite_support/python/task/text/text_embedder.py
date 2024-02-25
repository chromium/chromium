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
"""Text embedder task."""

import dataclasses

from tensorflow_lite_support.python.task.core import base_options as base_options_module
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embedding_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_text_embedder

_CppTextEmbedder = _pywrap_text_embedder.TextEmbedder
_BaseOptions = base_options_module.BaseOptions
_EmbeddingOptions = embedding_options_pb2.EmbeddingOptions


@dataclasses.dataclass
class TextEmbedderOptions:
  """Options for the text embedder task.

  Attributes:
    base_options: Base options for the text embedder task.
    embedding_options: Embedding options for the text embedder task.
  """
  base_options: _BaseOptions
  embedding_options: _EmbeddingOptions = dataclasses.field(
      default_factory=_EmbeddingOptions
  )


class TextEmbedder(object):
  """Class that performs dense feature vector extraction on text."""

  def __init__(self, options: TextEmbedderOptions,
               cpp_embedder: _CppTextEmbedder) -> None:
    """Initializes the `TextEmbedder` object."""
    # Creates the object of C++ TextEmbedder class.
    self._options = options
    self._embedder = cpp_embedder

  @classmethod
  def create_from_file(cls, file_path: str) -> "TextEmbedder":
    """Creates the `TextEmbedder` object from a TensorFlow Lite model.

    Args:
      file_path: Path to the model.

    Returns:
      `TextEmbedder` object that's created from the model file.
    Raises:
      ValueError: If failed to create `TextEmbedder` object from the provided
        file such as invalid file.
      RuntimeError: If other types of error occurred.
    """
    base_options = _BaseOptions(file_name=file_path)
    options = TextEmbedderOptions(base_options=base_options)
    return cls.create_from_options(options)

  @classmethod
  def create_from_options(cls, options: TextEmbedderOptions) -> "TextEmbedder":
    """Creates the `TextEmbedder` object from text embedder options.

    Args:
      options: Options for the text embedder task.

    Returns:
      `TextEmbedder` object that's created from `options`.
    Raises:
      ValueError: If failed to create `TextEmbedder` object from
        `TextEmbedderOptions` such as missing the model.
      RuntimeError: If other types of error occurred.
    """
    embedder = _CppTextEmbedder.create_from_options(
        options.base_options.to_pb2(), options.embedding_options.to_pb2())
    return cls(options, embedder)

  def embed(self, text: str) -> embedding_pb2.EmbeddingResult:
    """Performs actual feature vector extraction on the provided text.

    Args:
      text: the input text, used to extract the feature vectors.

    Returns:
      embedding result.

    Raises:
      ValueError: If any of the input arguments is invalid.
      RuntimeError: If failed to calculate the embedding vector.
    """
    embedding_result = self._embedder.embed(text)
    return embedding_pb2.EmbeddingResult.create_from_pb2(embedding_result)

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
  def options(self) -> TextEmbedderOptions:
    return self._options
