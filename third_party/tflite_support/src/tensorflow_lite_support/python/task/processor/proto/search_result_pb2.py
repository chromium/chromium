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
"""Search result protobuf."""

import dataclasses
from typing import Any, List

from tensorflow_lite_support.cc.task.processor.proto import search_result_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_NearestNeighborProto = search_result_pb2.NearestNeighbor
_SearchResultProto = search_result_pb2.SearchResult


@dataclasses.dataclass
class NearestNeighbor:
  """A single nearest neighbor.

  Attributes:
    metadata: User-defined metadata about the result. This could be a label, a
      unique ID, a serialized proto of some sort, etc.
    distance: The distance score indicating how confident the result is. Lower
      is better.
  """

  metadata: bytearray
  distance: float

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _NearestNeighborProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _NearestNeighborProto(
        metadata=bytes(self.metadata), distance=self.distance)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _NearestNeighborProto) -> "NearestNeighbor":
    """Creates a `NearestNeighbor` object from the given protobuf object."""
    return NearestNeighbor(
        metadata=bytearray(pb2_obj.metadata), distance=pb2_obj.distance)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, NearestNeighbor):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class SearchResult:
  """Results from a search as a list of nearest neigbors.

  Attributes:
    nearest_neighbors: The nearest neighbors, sorted by increasing distance
      order.
  """

  nearest_neighbors: List[NearestNeighbor]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _SearchResultProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _SearchResultProto(nearest_neighbors=[
        nearest_neighbor.to_pb2() for nearest_neighbor in self.nearest_neighbors
    ])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _SearchResultProto) -> "SearchResult":
    """Creates a `SearchResult` object from the given protobuf object."""
    return SearchResult(nearest_neighbors=[
        NearestNeighbor.create_from_pb2(nearest_neighbor)
        for nearest_neighbor in pb2_obj.nearest_neighbors
    ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, SearchResult):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
