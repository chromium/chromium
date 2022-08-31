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
"""CLU annotation options protobuf."""

import dataclasses
from typing import Any, Optional

from tensorflow_lite_support.cc.task.processor.proto import clu_annotation_options_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_BertCluAnnotationOptionsProto = clu_annotation_options_pb2.BertCluAnnotationOptions


@dataclasses.dataclass
class BertCluAnnotationOptions:
  """Options for Bert CLU Annotator processor.

  Attributes:
    max_history_turns: Max number of history turns to encode by the model.
    domain_threshold: The threshold of domain prediction.
    intent_threshold: The threshold of intent prediction.
    categorical_slot_threshold: The threshold of categorical slot prediction.
    mentioned_slot_threshold: The threshold of mentioned slot
      prediction.
  """

  max_history_turns: Optional[int] = 5
  domain_threshold: Optional[float] = 0.5
  intent_threshold: Optional[float] = 0.5
  categorical_slot_threshold: Optional[float] = 0.5
  mentioned_slot_threshold: Optional[float] = 0.5

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _BertCluAnnotationOptionsProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _BertCluAnnotationOptionsProto(
        max_history_turns=self.max_history_turns,
        domain_threshold=self.domain_threshold,
        intent_threshold=self.intent_threshold,
        categorical_slot_threshold=self.categorical_slot_threshold,
        mentioned_slot_threshold=self.mentioned_slot_threshold)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(
      cls,
      pb2_obj: _BertCluAnnotationOptionsProto) -> "BertCluAnnotationOptions":
    """Creates a `BertCluAnnotationOptions` object from the given protobuf object."""
    return BertCluAnnotationOptions(
        max_history_turns=pb2_obj.max_history_turns,
        domain_threshold=pb2_obj.domain_threshold,
        intent_threshold=pb2_obj.intent_threshold,
        categorical_slot_threshold=pb2_obj.categorical_slot_threshold,
        mentioned_slot_threshold=pb2_obj.mentioned_slot_threshold)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, BertCluAnnotationOptions):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
