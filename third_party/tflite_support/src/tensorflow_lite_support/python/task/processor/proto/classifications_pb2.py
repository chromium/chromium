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
"""Classifications protobuf."""

import dataclasses
from typing import Any, List

from tensorflow_lite_support.cc.task.processor.proto import classifications_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls
from tensorflow_lite_support.python.task.processor.proto import class_pb2

_ClassificationsProto = classifications_pb2.Classifications
_ClassificationResultProto = classifications_pb2.ClassificationResult


@dataclasses.dataclass
class Classifications:
  """List of predicted classes (aka labels) for a given classifier head.

  Attributes:
    categories: The array of predicted categories, usually sorted by descending
      scores (e.g. from high to low probability).
    head_index: The index of the classifier head these categories refer to. This
      is useful for multi-head models.
    head_name: The name of the classifier head, which is the corresponding
      tensor metadata.
  """

  categories: List[class_pb2.Category]
  head_index: int
  head_name: str

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _ClassificationsProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _ClassificationsProto(
        classes=[category.to_pb2() for category in self.categories],
        head_index=self.head_index,
        head_name=self.head_name)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _ClassificationsProto) -> "Classifications":
    """Creates a `Classifications` object from the given protobuf object."""
    return Classifications(
        categories=[
            class_pb2.Category.create_from_pb2(category)
            for category in pb2_obj.classes
        ],
        head_index=pb2_obj.head_index,
        head_name=pb2_obj.head_name)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, Classifications):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class ClassificationResult:
  """Contains one set of results per classifier head.

  Attributes:
    classifications: A list of `Classifications` objects.
  """

  classifications: List[Classifications]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _ClassificationResultProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _ClassificationResultProto(classifications=[
        classification.to_pb2() for classification in self.classifications
    ])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(
      cls, pb2_obj: _ClassificationResultProto) -> "ClassificationResult":
    """Creates a `ClassificationResult` object from the given protobuf object."""
    return ClassificationResult(classifications=[
        Classifications.create_from_pb2(classification)
        for classification in pb2_obj.classifications
    ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, ClassificationResult):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
