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
"""Search options protobuf."""

import dataclasses
from typing import Any, Optional

from tensorflow_lite_support.cc.task.core.proto import external_file_pb2

from tensorflow_lite_support.cc.task.processor.proto import search_options_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_ExternalFileProto = external_file_pb2.ExternalFile
_SearchOptionsProto = search_options_pb2.SearchOptions


@dataclasses.dataclass
class SearchOptions:
  """Options for search processor.

  The index file to search into. Mandatory only if the index is not attached
  to the output tensor metadata as an AssociatedFile with type SCANN_INDEX_FILE.
  The index file can be specified by one of the following two ways:

  (1) file contents loaded in `index_file_content`.
  (2) file path in `index_file_name`.

  If more than one field of these fields is provided, they are used in this
  precedence order.

  Attributes:
    index_file_name: Path to the index.
    index_file_content: The index file contents as bytes.
    max_results: Maximum number of nearest neighbor results to return.
  """

  index_file_name: Optional[str] = None
  index_file_content: Optional[bytes] = None
  max_results: Optional[int] = 5

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _SearchOptionsProto:
    """Generates a protobuf object to pass to the C++ layer."""
    if self.index_file_name is not None or self.index_file_content is not None:
      return _SearchOptionsProto(
          index_file=_ExternalFileProto(
              file_name=self.index_file_name,
              file_content=self.index_file_content),
          max_results=self.max_results)
    else:
      return _SearchOptionsProto(max_results=self.max_results)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _SearchOptionsProto) -> "SearchOptions":
    """Creates a `SearchOptionsProto` object from the given protobuf object."""
    return SearchOptions(
        index_file_name=pb2_obj.index_file.file_name,
        index_file_content=pb2_obj.index_file.file_content,
        max_results=pb2_obj.max_results)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, SearchOptions):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
