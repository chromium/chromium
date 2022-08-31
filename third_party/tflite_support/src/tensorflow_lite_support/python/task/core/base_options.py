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
"""Base options for task APIs."""

import dataclasses
from typing import Any, Optional

from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls
from tensorflow_lite_support.python.task.core.proto import base_options_pb2

_BaseOptionsProto = base_options_pb2.BaseOptions


@dataclasses.dataclass
class BaseOptions:
  """Base options for TensorFlow Lite Task Library's Python APIs.

  Represents external files used by the Task APIs (e.g. TF Lite FlatBuffer or
  plain-text labels file). The files can be specified by one of the following
  two ways:

  (1) file contents loaded in `file_content`.
  (2) file path in `file_name`.

  If more than one field of these fields is provided, they are used in this
  precedence order.

  Attributes:
    file_name: Path to the index.
    file_content: The index file contents as bytes.
    num_threads: Number of thread, the default value is -1 which means
      Interpreter will decide what is the most appropriate `num_threads`.
    use_coral: If true, inference will be delegated to a connected Coral Edge
      TPU device.
  """

  file_name: Optional[str] = None
  file_content: Optional[bytes] = None
  num_threads: Optional[int] = -1
  use_coral: Optional[bool] = None

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _BaseOptionsProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _BaseOptionsProto(
        file_name=self.file_name,
        file_content=self.file_content,
        num_threads=self.num_threads,
        use_coral=self.use_coral)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _BaseOptionsProto) -> "BaseOptions":
    """Creates a `BaseOptions` object from the given protobuf object."""
    return BaseOptions(
        file_name=pb2_obj.file_name,
        file_content=pb2_obj.file_content,
        num_threads=pb2_obj.num_threads,
        use_coral=pb2_obj.use_coral)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, BaseOptions):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
