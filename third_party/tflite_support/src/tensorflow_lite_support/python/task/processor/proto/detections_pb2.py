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
"""Detections protobuf."""

import dataclasses
from typing import Any, List

from tensorflow_lite_support.cc.task.processor.proto import detections_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls
from tensorflow_lite_support.python.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.processor.proto import class_pb2

_DetectionProto = detections_pb2.Detection
_DetectionResultProto = detections_pb2.DetectionResult


@dataclasses.dataclass
class Detection:
  """Represents one detected object in the object detector's results.

  Attributes:
    bounding_box: A `bounding_box_pb2.BoundingBox` object.
    categories: A list of `class_pb2.Category` objects.
  """

  bounding_box: bounding_box_pb2.BoundingBox
  categories: List[class_pb2.Category]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _DetectionProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _DetectionProto(
        bounding_box=self.bounding_box.to_pb2(),
        classes=[category.to_pb2() for category in self.categories])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _DetectionProto) -> "Detection":
    """Creates a `Detection` object from the given protobuf object."""
    return Detection(
        bounding_box=bounding_box_pb2.BoundingBox.create_from_pb2(
            pb2_obj.bounding_box),
        categories=[
            class_pb2.Category.create_from_pb2(category)
            for category in pb2_obj.classes
        ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, Detection):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class DetectionResult:
  """Represents the list of detected objects.

  Attributes:
    detections: A list of `Detection` objects.
  """

  detections: List[Detection]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _DetectionResultProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _DetectionResultProto(
        detections=[detection.to_pb2() for detection in self.detections])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _DetectionResultProto) -> "DetectionResult":
    """Creates a `DetectionResult` object from the given protobuf object."""
    return DetectionResult(detections=[
        Detection.create_from_pb2(detection) for detection in pb2_obj.detections
    ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, DetectionResult):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
