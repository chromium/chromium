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
"""Embedding result protobuf."""

import dataclasses
from typing import Any, List

import numpy as np
from tensorflow_lite_support.cc.task.processor.proto import embedding_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_FeatureVectorProto = embedding_pb2.FeatureVector
_EmbeddingProto = embedding_pb2.Embedding
_EmbeddingResultProto = embedding_pb2.EmbeddingResult


@dataclasses.dataclass
class FeatureVector:
  """A dense feature vector.

  Only one of the two fields is ever present.
  Feature vectors are assumed to be one-dimensional and L2-normalized.

  Attributes:
    value: A NumPy array indidcating the raw output of the embedding layer. The
      datatype of elements in the array can be either float or uint8 if
      `quantize` is set to True in `EmbeddingOptions`.
  """

  value: np.ndarray

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _FeatureVectorProto:
    """Generates a protobuf object to pass to the C++ layer."""

    if self.value.dtype == float:
      return _FeatureVectorProto(value_float=self.value)

    elif self.value.dtype == np.uint8:
      return _FeatureVectorProto(value_string=bytes(self.value))

    else:
      raise ValueError("Invalid dtype. Only float and np.uint8 are supported.")

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _FeatureVectorProto) -> "FeatureVector":
    """Creates a `FeatureVector` object from the given protobuf object."""

    if pb2_obj.value_float:
      return FeatureVector(
          value=np.array(pb2_obj.value_float, dtype=float))

    elif pb2_obj.value_string:
      return FeatureVector(
          value=np.array(bytearray(pb2_obj.value_string), dtype=np.uint8))

    else:
      raise ValueError("Either value_float or value_string must exist.")

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, FeatureVector):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class Embedding:
  """Result produced by one of the embedder model output layers.

  Attributes:
    feature_vector: The output feature vector.
    output_index: The index of the model output layer that produced this feature
      vector.
  """

  feature_vector: FeatureVector
  output_index: int

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _EmbeddingProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _EmbeddingProto(
        feature_vector=self.feature_vector.to_pb2(),
        output_index=self.output_index)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _EmbeddingProto) -> "Embedding":
    """Creates a `Embedding` object from the given protobuf object."""
    return Embedding(
        feature_vector=FeatureVector.create_from_pb2(pb2_obj.feature_vector),
        output_index=pb2_obj.output_index)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, Embedding):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class EmbeddingResult:
  """Embeddings produced by the Embedder.

  Attributes:
    embeddings: The embeddings produced by each of the model output layers.
      Except in advanced cases, the embedding model has a single output layer,
      and this list is thus made of a single element feature vector.
  """

  embeddings: List[Embedding]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _EmbeddingResultProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _EmbeddingResultProto(
        embeddings=[embedding.to_pb2() for embedding in self.embeddings])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _EmbeddingResultProto) -> "EmbeddingResult":
    """Creates a `EmbeddingResult` object from the given protobuf object."""
    return EmbeddingResult(embeddings=[
        Embedding.create_from_pb2(embedding) for embedding in pb2_obj.embeddings
    ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, EmbeddingResult):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
