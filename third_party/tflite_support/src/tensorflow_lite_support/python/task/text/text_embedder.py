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
from typing import Any, Optional

from tensorflow_lite_support.python.task.core import task_options
from tensorflow_lite_support.python.task.core import task_utils
from tensorflow_lite_support.python.task.processor.proto import embedding_options_pb2
from tensorflow_lite_support.python.task.processor.proto import embeddings_pb2
from tensorflow_lite_support.python.task.text.pybinds import _pywrap_text_embedder
from tensorflow_lite_support.python.task.text.pybinds import text_embedder_options_pb2

_ProtoTextEmbedderOptions = text_embedder_options_pb2.TextEmbedderOptions
_CppTextEmbedder = _pywrap_text_embedder.TextEmbedder


@dataclasses.dataclass
class TextEmbedderOptions:
  """Options for the text embedder task."""
  base_options: task_options.BaseOptions
  embedding_options: Optional[embedding_options_pb2.EmbeddingOptions] = None

  def __eq__(self, other: Any) -> bool:
    if (not isinstance(other, self.__class__) or
        self.base_options != other.base_options):
      return False

    if self.embedding_options is None and other.embedding_options is None:
      return True
    elif (self.embedding_options and other.embedding_options and
          self.embedding_options.SerializeToString()
          == self.embedding_options.SerializeToString()):
      return True
    else:
      return False


class TextEmbedder(object):
  """Class that performs dense feature vector extraction on text."""

  def __init__(self, options: TextEmbedderOptions,
               cpp_embedder: _CppTextEmbedder) -> None:
    """Initializes the `TextEmbedder` object."""
    self._options = options
    self._embedder = cpp_embedder

  @classmethod
  def create_from_options(cls, options: TextEmbedderOptions) -> "TextEmbedder":
    """Creates the `TextEmbedder` object from text embedder options.

    Args:
      options: Options for the text embedder task.

    Returns:
      `TextEmbedder` object that's created from `options`.

    Raises:
      TODO(b/220931229): Raise RuntimeError instead of status.StatusNotOk.
      status.StatusNotOk if failed to create `TextEmbdder` object from
        `TextEmbedderOptions` such as missing the model. Need to import the
        module to catch this error: `from pybind11_abseil import status`, see
        https://github.com/pybind/pybind11_abseil#abslstatusor.
    """
    # Creates the object of C++ TextEmbedder class.
    proto_options = _ProtoTextEmbedderOptions()
    proto_options.base_options.CopyFrom(
        task_utils.ConvertToProtoBaseOptions(options.base_options))
    if options.embedding_options:
      embedding_options = proto_options.embedding_options.add()
      embedding_options.CopyFrom(options.embedding_options)
    embedder = _CppTextEmbedder.create_from_options(proto_options)
    return cls(options, embedder)

  def embed(self, text: str) -> embeddings_pb2.EmbeddingResult:
    """Performs actual feature vector extraction on the provided text.

    Args:
      text: the input text, used to extract the feature vectors.

    Returns:
      embedding result.

    Raises:
      status.StatusNotOk if failed to get the embedding vector. Need to import
        the module to catch this error: `from pybind11_abseil import status`,
        see https://github.com/pybind/pybind11_abseil#abslstatusor.
    """
    return self._embedder.embed(text)

  def cosine_similarity(self, u: embeddings_pb2.FeatureVector,
                        v: embeddings_pb2.FeatureVector) -> float:
    """Computes cosine similarity [1] between two feature vectors."""
    return self._embedder.cosine_similarity(u, v)

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

  def __eq__(self, other: Any) -> bool:
    return (isinstance(other, self.__class__) and
            self._options == other._options)

  @property
  def options(self) -> TextEmbedderOptions:
    return self._options
