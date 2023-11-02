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
"""CLU protobuf."""

import dataclasses
from typing import Any, List

from tensorflow_lite_support.cc.task.processor.proto import clu_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls
from tensorflow_lite_support.python.task.processor.proto import class_pb2

_CluRequestProto = clu_pb2.CluRequest
_CluResponseProto = clu_pb2.CluResponse
_CategoricalSlotProto = clu_pb2.CategoricalSlot
_MentionProto = clu_pb2.Mention
_MentionedSlotProto = clu_pb2.MentionedSlot


@dataclasses.dataclass
class CluRequest:
  """The input to CLU (Conversational Language Understanding).

  Attributes:
    utterances: The utterances of dialogue conversation turns in the
      chronological order. The last utterance is the current turn.
  """

  utterances: List[str]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _CluRequestProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _CluRequestProto(utterances=self.utterances)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _CluRequestProto) -> "CluRequest":
    """Creates a `CluRequest` object from the given protobuf object."""
    return CluRequest(
        utterances=[str(utterance) for utterance in pb2_obj.utterances])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, CluRequest):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class CategoricalSlot:
  """Represents a categorical slot whose values are within a finite set.

  Attributes:
    slot: The name of the slot.
    prediction: The predicted class.
  """

  slot: str
  prediction: class_pb2.Category

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _CategoricalSlotProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _CategoricalSlotProto(
        slot=self.slot, prediction=self.prediction.to_pb2())

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _CategoricalSlotProto) -> "CategoricalSlot":
    """Creates a `CategoricalSlot` object from the given protobuf object."""
    print(pb2_obj)
    return CategoricalSlot(
        slot=pb2_obj.slot,
        prediction=class_pb2.Category.create_from_pb2(pb2_obj.prediction))

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, CategoricalSlot):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class Mention:
  """A single mention result.

  Attributes:
    value: The text value of the mention.
    score: The score for this mention e.g. (but not necessarily) a
      probability in [0,1].
    start: Start of the bytes of this mention.
    end: Exclusive end of the bytes of this mention.
  """

  value: str
  score: float
  start: int
  end: int

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _MentionProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _MentionProto(
        value=self.value, score=self.score, start=self.start, end=self.end)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _MentionProto) -> "Mention":
    """Creates a `Mention` object from the given protobuf object."""
    return Mention(
        value=pb2_obj.value,
        score=pb2_obj.score,
        start=pb2_obj.start,
        end=pb2_obj.end)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, Mention):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class MentionedSlot:
  """Non-categorical slot whose values are open text extracted from the input text.

  Attributes:
    slot: The name of the slot.
    mention: The predicted mention.
  """

  slot: str
  mention: Mention

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _MentionedSlotProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _MentionedSlotProto(
        slot=self.slot, mention=self.mention.to_pb2())

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(
      cls, pb2_obj: _MentionedSlotProto) -> "MentionedSlot":
    """Creates a `MentionedSlot` object from the given protobuf object."""
    return MentionedSlot(
        slot=pb2_obj.slot,
        mention=Mention.create_from_pb2(pb2_obj.mention))

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, MentionedSlot):
      return False

    return self.to_pb2().__eq__(other.to_pb2())


@dataclasses.dataclass
class CluResponse:
  """The output of CLU.

  Attributes:
    domains: The list of predicted domains.
    intents: The list of predicted intents.
    categorical_slots: The list of predicted categorical slots.
    mentioned_slots: The list of predicted mentioned slots.
  """

  domains: List[class_pb2.Category]
  intents: List[class_pb2.Category]
  categorical_slots: List[CategoricalSlot]
  mentioned_slots: List[MentionedSlot]

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _CluResponseProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _CluResponseProto(
        domains=[domain.to_pb2() for domain in self.domains],
        intents=[intent.to_pb2() for intent in self.intents],
        categorical_slots=[
            categorical_slot.to_pb2()
            for categorical_slot in self.categorical_slots
        ],
        mentioned_slots=[
            mentioned_slot.to_pb2()
            for mentioned_slot in self.mentioned_slots
        ])

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls, pb2_obj: _CluResponseProto) -> "CluResponse":
    """Creates a `CluResponse` object from the given protobuf object."""
    return CluResponse(
        domains=[
            class_pb2.Category.create_from_pb2(domain)
            for domain in pb2_obj.domains
        ],
        intents=[
            class_pb2.Category.create_from_pb2(intent)
            for intent in pb2_obj.intents
        ],
        categorical_slots=[
            CategoricalSlot.create_from_pb2(categorical_slot)
            for categorical_slot in pb2_obj.categorical_slots
        ],
        mentioned_slots=[
            MentionedSlot.create_from_pb2(mentioned_slot)
            for mentioned_slot in pb2_obj.mentioned_slots
        ])

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, CluResponse):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
