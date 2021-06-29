#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import copy
import logging
import os
import platform
import re
import subprocess
import sys
import traceback

from enum import Enum, auto
from functools import reduce
from google.protobuf import text_format
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message
from typing import NewType, TYPE_CHECKING, Any, Optional, List, Dict, Set, NamedTuple, Tuple, Union

# Path to the directory where this script is.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

# Absolute path to chrome/src.
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../../.."))

# TODO(nicolaso): Move extractor.py to this folder once the C++ auditor doesn't
# depend on it anymore.
sys.path.insert(0, os.path.join(SCRIPT_DIR, ".."))
import extractor

if TYPE_CHECKING:
  # For the `mypy` type checker, a hardcoded import that is never used when
  # actually running. The real import is in AuditorUI.import_proto()
  #
  # TODO(nicolaso): Add instructions for running mypy.
  import traffic_annotation_pb2
  from traffic_annotation_pb2 import NetworkTrafficAnnotation as \
      traffic_annotation

UniqueId = NewType("UniqueId", str)
HashCode = NewType("HashCode", int)

# Reserved annotation unique IDs that should only be used in untracked files
# (e.g., test files or files that aren't compiled on this platform).
TEST_IDS = [UniqueId("test"), UniqueId("test_partial")]
MISSING_ID = UniqueId("missing")
NO_ANNOTATION_ID = UniqueId("undefined")
RESERVED_IDS = TEST_IDS + [MISSING_ID, NO_ANNOTATION_ID]

# Host platforms that support running auditor.py.
SUPPORTED_PLATFORMS = ["linux", "windows"]

logging.basicConfig(level=logging.INFO,
                    format="%(filename)s:%(lineno)d:%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)


def twos_complement_8bit(b: int) -> int:
  """Interprets b like a signed 8-bit integer, possibly changing its sign.

  For instance, twos_complement_8bit(204) returns -52."""
  if b >= 256:
    raise ValueError("b must fit inside 8 bits")
  if b & (1 << 7):
    # Negative number, calculate its value using two's-complement.
    return b - (1 << 8)
  else:
    # Positive number, do not touch.
    return b


def iterative_hash(s: str) -> HashCode:
  """Compute the has code of the given string as in:
  net/traffic_annotation/network_traffic_annotation.h

  Args:
    s: str
      The seed, e.g. unique id of traffic annotation.
  Returns: int
    A hash code.
  """
  return HashCode(
      reduce(lambda acc, b: (acc * 31 + twos_complement_8bit(b)) % 138003713,
             s.encode("utf-8"), 0))


def compute_hash_value(text: str) -> HashCode:
  """Same as iterative_hash, but returns -1 for empty strings."""
  return iterative_hash(text) if text else HashCode(-1)


def merge_string_field(src: Message, dst: Message, field: str):
  """Merges the content of one string field into an annotation."""
  if getattr(src, field):
    if getattr(dst, field):
      setattr(dst, field, "{}\n{}".format(getattr(src, field),
                                          getattr(dst, field)))
    else:
      setattr(dst, field, getattr(src, field))


def fill_proto_with_bogus(proto: Message, field_numbers: List[int]):
  """Fill proto with bogus values for the fields identified by field_numbers.
  Uses reflection to fill the proto with the right types."""
  descriptor = proto.DESCRIPTOR
  for field_number in field_numbers:
    field_number = abs(field_number)

    if field_number not in descriptor.fields_by_number:
      raise ValueError("{} is not a valid {} field".format(
          field_number, descriptor.name))

    field = descriptor.fields_by_number[field_number]
    repeated = field.label == FieldDescriptor.LABEL_REPEATED

    if field.type == FieldDescriptor.TYPE_STRING and not repeated:
      setattr(proto, field.name, "[Archived]")
    elif field.type == FieldDescriptor.TYPE_ENUM and not repeated:
      # Assume the 2nd value in the enum is reasonable, since the 1st is
      # UNSPECIFIED.
      setattr(proto, field.name, field.enum_type.values[1].number)
    elif field.type == FieldDescriptor.TYPE_MESSAGE and repeated:
      getattr(proto, field.name).add()
    else:
      raise NotImplementedError("Unimplemented proto field type {} ({})".format(
          field.type, "repeated" if repeated else "non-repeated"))


class AuditorError:
  class Type(Enum):
    # Annotation syntax is not right.
    SYNTAX = auto()
    # Can't create a MutableNetworkTrafficAnnotationTag from anywhere (except
    # in whitelisted files).
    # TODO(nicolaso): Implement this error type.
    MUTABLE_TAG = auto()
    # Annotation has some missing fields.
    INCOMPLETE_ANNOTATION = auto()
    # Annotation has some inconsistent fields.
    INCONSISTENT_ANNOTATION = auto()
    # Two annotations that are supposed to merge cannot merge.
    MERGE_FAILED = auto()
    # A function is called with the "missing" tag.
    MISSING_TAG_USED = auto()
    # A function is called with the "test" or "test_partial" tag outside of a
    # test file.
    TEST_ANNOTATION = auto()
    # A function is called with NO_TRAFFIC_ANNOTATION_YET tag.
    NO_ANNOTATION = auto()
    # An id has a hash code equal to a reserved word.
    RESERVED_ID_HASH_CODE = auto()
    # An id has a hash code equal to a deprecated one.
    DEPRECATED_ID_HASH_CODE = auto()
    # An id contains an invalid character (not alphanumeric or underscore).
    ID_INVALID_CHARACTER = auto()
    # An id is used in two places without matching conditions. Proper conditions
    # include when 2 annotations are completing each other, or are different
    # branches of the same annotation.
    REPEATED_ID = auto()
    # Annotation does not have a valid second id.
    MISSING_SECOND_ID = auto()
    # Two ids have equal hash codes.
    HASH_CODE_COLLISION = auto()

  def __init__(self,
               result_type: "AuditorError.Type",
               message: str = "",
               file_path: str = "",
               line: int = 0,
               *extra_details: str):
    self.type = result_type
    self.message = message
    self.file_path = file_path
    self.line = line
    self._details = []

    assert message or result_type in [
        AuditorError.Type.MISSING_TAG_USED, AuditorError.Type.TEST_ANNOTATION,
        AuditorError.Type.NO_ANNOTATION, AuditorError.Type.MISSING_SECOND_ID,
        AuditorError.Type.MUTABLE_TAG
    ]

    if message:
      self._details.append(message)
    self._details.extend(extra_details)

  def __str__(self) -> str:
    #TODO(https://crbug.com/1119417): Add all the possible errors and their
    # explanations here.
    if self.type == AuditorError.Type.SYNTAX:
      assert self._details
      return ("SYNTAX: Annotation at '{}:{}' has the following syntax"
              " error: {}".format(self.file_path, self.line,
                                  str(self._details[0]).replace("\n", " ")))

    if self.type == AuditorError.Type.MUTABLE_TAG:
      return ("MUTABLE_TAG: Calling CreateMutableNetworkTrafficAnnotationTag() "
              "is not safelisted at '{}:{}'.".format(self.file_path, self.line))

    if self.type == AuditorError.Type.INCOMPLETE_ANNOTATION:
      assert self._details
      return ("INCOMPLETE_ANNOTATION: Annotation at '{}:{}' has the"
              " following missing fields: {}".format(self.file_path, self.line,
                                                     self._details[0]))

    if self.type == AuditorError.Type.INCONSISTENT_ANNOTATION:
      assert self._details
      return ("INCONSISTENT_ANNOTATION: Annotation at '{}:{}' has the "
              "following inconsistencies: {}".format(self.file_path, self.line,
                                                     self._details[0]))
    if self.type == AuditorError.Type.MERGE_FAILED:
      assert len(self._details) == 3
      return ("MERGE_FAILED: Annotations '{}' and '{}' cannot be merged due to "
              "the following error(s): {}".format(self._details[1],
                                                  self._details[2],
                                                  self._details[0]))

    if self.type == AuditorError.Type.MISSING_TAG_USED:
      return ("MISSING_TAG_USED: MISSING_TRAFFIC_ANNOTATION tag used in "
              "'{}:{}'.".format(self.file_path, self.line))

    if self.type == AuditorError.Type.TEST_ANNOTATION:
      return ("TEST_ANNOTATION: Annotation for tests used in '{}:{}'.".format(
          self.file_path, self.line))

    if self.type == AuditorError.Type.NO_ANNOTATION:
      # TODO(nicolaso): We should ignore this error on Android for now.
      return "NO_ANNOTATION: Empty annotation in '{}:{}'.".format(
          self.file_path, self.line)

    if self.type == AuditorError.Type.RESERVED_ID_HASH_CODE:
      assert self._details
      return ("RESERVED_ID_HASH_CODE: Id '{}' in '{}:{}' has a hash code equal "
              "to a reserved word and should be changed.".format(
                  self._details[0], self.file_path, self.line))

    if self.type == AuditorError.Type.DEPRECATED_ID_HASH_CODE:
      assert self._details
      return ("DEPRECATED_ID_HASH_CODE: Id '{}' in '{}:{}' has a hash code "
              "equal to a deprecated id and should be changed.".format(
                  self._details[0], self.file_path, self.line))

    if self.type == AuditorError.Type.HASH_CODE_COLLISION:
      assert len(self._details) == 2
      return ("HASH_CODE_COLLISION: The following annotations have colliding "
              "hash codes and should be updated: '{}', '{}'.".format(
                  self._details[0], self._details[1]))

    if self.type == AuditorError.Type.REPEATED_ID:
      assert len(self._details) == 2
      return ("REPEATED_ID: The following annotations have equal ids and "
              "should be updated: {}, {}.".format(self._details[0],
                                                  self._details[1]))

    if self.type == AuditorError.Type.ID_INVALID_CHARACTER:
      assert self._details
      return ("ID_INVALID_CHARACTER: Id '{}' in '{}:{}' contains an invalid "
              "character.".format(self._details[0], self.file_path, self.line))

    if self.type == AuditorError.Type.MISSING_SECOND_ID:
      return ("MISSING_SECOND_ID: Second id of annotation at '{}:{}' should be "
              "updated, as it has the same hash code as the first one.".format(
                  self.file_path, self.line))

    raise NotImplementedError("Unimplemented AuditorError.Type: {}".format(
        self.type.name))

  def __repr__(self) -> str:
    return "AuditorError(\"{}\")".format(str(self))


class Annotation:
  """An annotation in code, typically extracted from C++.

  Attributes:
    type: An Annotation.Type with the kind of annotation this is.
    proto: A NetworkTrafficAnnotation protobuf message.

    unique_id: The unique ID for this annotation/proto.
    unique_id_hash_code: HashCode of the unique_id.

    second_id: A UniqueId with the other annotation's unique id. This can be the
        completing id for partial annotations, or group id for branched
        completing annotations.
    second_id_hash_code: HashCode of the second_id.

    is_loaded_from_archive: True if this annotations was loaded from
        annotations.xml, rather than extracted from C++ code.
    archived_content_hash_code: content_hash_code loaded from annotations.xml.
    archived_added_in_milestone: added_in_milestone from annotations.xml.

    is_merged: True if this annotation was generated by merging 2 other
       incomplete annotations.
  """

  class Type(Enum):
    COMPLETE = (0, "Definition")
    PARTIAL = (1, "Partial")
    COMPLETING = (2, "Completing")
    BRANCHED_COMPLETING = (3, "BranchedCompleting")

    def __str__(self):
      return self.value[1]

    @classmethod
    def from_int(cls, n: int) -> "Annotation.Type":
      for t in Annotation.Type:
        if t.value[0] == n:
          return t
      raise ValueError("{} is not a valid Annotation.Type".format(n))

    @classmethod
    def from_string(cls, name: str) -> "Annotation.Type":
      for t in Annotation.Type:
        if t.value[1] == name:
          return t
      raise ValueError("'{}' is not a valid Annotation.Type".format(name))

  def __init__(self):
    self.type = Annotation.Type.COMPLETING
    self.proto = traffic_annotation_pb2.NetworkTrafficAnnotation()

    # TODO(nicolaso): Remove hash_code="" from annotations.xml, and instead
    # compute its value from unique_id.

    self.second_id: UniqueId = ""
    self.second_id_hash_code: HashCode = -1
    # TODO(nicolaso): Store the second_id instead of its hashcode in
    # annotations.xml. Then, make second_id_hash_code a computed property like
    # unique_id_hash_code.

    self.is_loaded_from_archive = False
    self.archived_content_hash_code: HashCode = -1
    self.archived_added_in_milestone = 0

    self.is_merged = False

  @property
  def unique_id(self) -> UniqueId:
    # Transparently expose the unique_id stored in the proto for convenience.
    return self.proto.unique_id

  @unique_id.setter
  def unique_id(self, unique_id: UniqueId):
    # Transparently expose the unique_id stored in the proto for convenience.
    self.proto.unique_id = unique_id

  @property
  def unique_id_hash_code(self) -> HashCode:
    return compute_hash_value(self.unique_id)

  def get_ids(self) -> List[Tuple[UniqueId, HashCode]]:
    """Returns the ids/hashcodes used by this annotation (up to 2 tuples)."""
    if self.needs_two_ids():
      return [
          (self.unique_id, self.unique_id_hash_code),
          (self.second_id, self.second_id_hash_code),
      ]
    else:
      return [(self.unique_id, self.unique_id_hash_code)]

  @classmethod
  def load_from_archive(cls, archived: "ArchivedAnnotation") -> "Annotation":
    """Loads an annotation based on the data from annotations.xml."""
    annotation = Annotation()
    annotation.is_loaded_from_archive = True
    annotation.type = Annotation.Type.from_int(archived.type)
    annotation.unique_id = archived.unique_id
    annotation.proto.source.file = archived.file_path
    annotation.archived_content_hash_code = archived.content_hash_code
    annotation.archived_added_in_milestone = archived.added_in_milestone

    if annotation.needs_two_ids():
      # We don't have the actual second id, so write a generated value to make
      # it non-empty. This is only relevant in tests.
      annotation.second_id = UniqueId("ARCHIVED_ID_{}".format(
          annotation.second_id_hash_code))
      annotation.second_id_hash_code = archived.second_id_hash_code

    fill_proto_with_bogus(annotation.proto.semantics, archived.semantics_fields)

    fill_proto_with_bogus(annotation.proto.policy, archived.policy_fields)

    # cookies_allowed is a special field: negative values indicate NO, and
    # positive values indicate YES.
    CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed
    policy_fields = archived.policy_fields
    policy_descriptor = annotation.proto.policy.DESCRIPTOR
    cookies_allowed_id = (
        policy_descriptor.fields_by_name["cookies_allowed"].number)
    if +cookies_allowed_id in archived.policy_fields:
      annotation.proto.policy.cookies_allowed = CookiesAllowed.YES
    if -cookies_allowed_id in archived.policy_fields:
      annotation.proto.policy.cookies_allowed = CookiesAllowed.NO

    return annotation

  def create_complete_annotation(self, completing_annotation: "Annotation"
                                 ) -> Tuple["Annotation", List[AuditorError]]:
    """Combines |self| partial annotation with a completing/branched_completing
    annotation and returns the combined complete annotation."""
    if not self.is_completable_with(completing_annotation):
      raise ValueError("{} is not completable with {}".format(
          self.unique_id, completing_annotation.unique_id))

    # To keep the source information meta data, if completing annotation is of
    # type COMPLETING, keep |self| as the main and the other as completing.
    # But if completing annotation is of type BRANCHED_COMPLETING, reverse
    # the order.
    combination = Annotation()
    if completing_annotation.type == Annotation.Type.COMPLETING:
      combination = copy.copy(self)
      combination.proto = traffic_annotation_pb2.NetworkTrafficAnnotation()
      combination.proto.MergeFrom(self.proto)
      other = completing_annotation
    else:
      combination = copy.copy(completing_annotation)
      combination.proto = traffic_annotation_pb2.NetworkTrafficAnnotation()
      combination.proto.MergeFrom(completing_annotation.proto)
      other = self

    combination.is_merged = True
    combination.type = Annotation.Type.COMPLETE
    combination.second_id = UniqueId("")
    combination.second_id_hash_code = HashCode(-1)

    # Update comment.
    merge_string_field(combination.proto, other.proto, "comments")
    combination.proto.comments += (
        "This annotation is a merge of the following two annotations:\n"
        "'{}' in '{}:{}' and '{}' in '{}:{}'".format(
            self.unique_id, self.proto.source.file, self.proto.source.line,
            completing_annotation.unique_id,
            completing_annotation.proto.source.file,
            completing_annotation.proto.source.line))

    # Copy TrafficSemantics.
    semantics_string_fields = [
        "sender", "description", "trigger", "data", "destination_other"
    ]
    for f in semantics_string_fields:
      merge_string_field(combination.proto.semantics, other.proto.semantics, f)

    # Merge 'destination' field.
    Destination = traffic_annotation.TrafficSemantics.Destination
    if combination.proto.semantics.destination == Destination.UNSPECIFIED:
      combination.proto.semantics.destination = (
          other.proto.semantics.destination)
    elif (other.proto.semantics.destination != Destination.UNSPECIFIED
          and other.proto.semantics.destination !=
          combination.proto.semantics.destination):
      return combination, [
          AuditorError(
              AuditorError.Type.MERGE_FAILED,
              "Annotations contain different semantics::destination values", "",
              0, self.unique_id, completing_annotation.unique_id)
      ]

    # Copy TrafficPolicy.
    policy_string_fields = [
        "cookies_store", "setting", "policy_exception_justification"
    ]
    for f in policy_string_fields:
      merge_string_field(combination.proto.policy, other.proto.policy, f)

    combination.proto.policy.cookies_allowed = max(
        combination.proto.policy.cookies_allowed,
        other.proto.policy.cookies_allowed)

    combination.proto.policy.chrome_policy.extend(
        other.proto.policy.chrome_policy)

    return combination, []

  def needs_two_ids(self) -> bool:
    """Tells if the annotation requires two ids. All annotations have a unique
    id, but partial annotations also require a completing id, and branched
    completing annotations require a group id."""
    return (self.type in [
        Annotation.Type.PARTIAL, Annotation.Type.BRANCHED_COMPLETING
    ])

  def is_completable_with(self, other) -> bool:
    """Checks to see if this annotation can be completed with the |other|
    annotation, based on their unique ids, types, and extra ids. |self| should
    be of partial type and the |other| either COMPLETING or BRANCHED_COMPLETING
    type."""
    if self.type != Annotation.Type.PARTIAL or not self.second_id:
      return False
    if other.type == Annotation.Type.COMPLETING:
      return self.second_id_hash_code == other.unique_id_hash_code
    if other.type == Annotation.Type.BRANCHED_COMPLETING:
      return self.second_id_hash_code == other.second_id_hash_code
    return False

  def get_semantics_field_numbers(self) -> List[int]:
    """Returns the proto field numbers of TrafficSemantics fields that are
    included in this annotation."""
    return [
        f.number for f in traffic_annotation.TrafficSemantics.DESCRIPTOR.fields
        if getattr(self.proto.semantics, f.name)
    ]

  def get_policy_field_numbers(self) -> List[int]:
    """Returns the proto field numbers of TrafficPolicy fields that are
    included in this annotation."""
    field_numbers = [
        f.number for f in traffic_annotation.TrafficPolicy.DESCRIPTOR.fields
        if getattr(self.proto.policy, f.name)
    ]

    # CookiesAllowed.NO is indicated with a negative value.
    CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed
    policy_descriptor = self.proto.policy.DESCRIPTOR
    cookies_allowed_id = (
        policy_descriptor.fields_by_name["cookies_allowed"].number)
    if self.proto.policy.cookies_allowed == CookiesAllowed.NO:
      field_numbers.remove(+cookies_allowed_id)
      field_numbers.insert(0, -cookies_allowed_id)

    return field_numbers

  def get_content_hash_code(self) -> HashCode:
    """Computes a hashcode for the annotation content. Source field is not used
    in this computation, as we don't need sensitivity to change in source
    location (file path + line number)."""
    if self.is_loaded_from_archive:
      return self.archived_content_hash_code

    source_free_proto = copy.deepcopy(self.proto)
    source_free_proto.ClearField("source")
    source_free_proto = text_format.MessageToString(source_free_proto,
                                                    as_utf8=True)
    return compute_hash_value(source_free_proto)

  def deserialize(self, serialized_annotation: extractor.Annotation
                  ) -> List[AuditorError]:
    """Deserializes an instance from extractor.Annotation."""
    file_path = os.path.relpath(serialized_annotation.file_path, SRC_DIR)
    line_number = serialized_annotation.line_number
    self.proto.source.file = file_path
    self.proto.source.line = line_number

    self.type = Annotation.Type.from_string(serialized_annotation.type_name)
    self.unique_id = serialized_annotation.unique_id
    self.second_id = serialized_annotation.extra_id
    self.second_id_hash_code = compute_hash_value(self.second_id)

    # Check for reserved IDs first, before trying to parse the Proto.
    if self.unique_id in TEST_IDS:
      return [
          AuditorError(AuditorError.Type.TEST_ANNOTATION, "", file_path,
                       line_number)
      ]

    if self.unique_id == MISSING_ID:
      return [
          AuditorError(AuditorError.Type.MISSING_TAG_USED, "", file_path,
                       line_number)
      ]

    if self.unique_id == NO_ANNOTATION_ID:
      return [
          AuditorError(AuditorError.Type.NO_ANNOTATION, "", file_path,
                       line_number)
      ]

    try:
      text_format.Parse(serialized_annotation.text, self.proto)
    except Exception as e:
      logger.error(str(e))
      return [
          AuditorError(AuditorError.Type.SYNTAX, str(e), file_path, line_number)
      ]

    return []

  def check_complete(self) -> List[AuditorError]:
    """Checks if an annotation has all required fields."""
    CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed

    unspecifieds = []
    # Check semantic fields.
    semantics_fields = [
        "sender", "description", "trigger", "data", "destination"
    ]
    for field in semantics_fields:
      if not getattr(self.proto.semantics, field):
        unspecifieds.append(field)

    # Check policy fields.
    policy = self.proto.policy
    # cookies_allowed must be specified.
    if policy.cookies_allowed == CookiesAllowed.UNSPECIFIED:
      unspecifieds.append("cookies_allowed")

    # cookies_store is only needed if CookiesAllowed.YES.
    if (not policy.cookies_store
        and policy.cookies_allowed == CookiesAllowed.YES):
      unspecifieds.append("cookies_store")

    # If either of 'chrome_policy' or 'policy_exception_justification' are
    # available, ignore not having the other one.
    if not policy.chrome_policy and not policy.policy_exception_justification:
      unspecifieds.append("chrome_policy")
      unspecifieds.append("policy_exception_justification")

    if unspecifieds:
      error_text = ", ".join(unspecifieds)
      return [
          AuditorError(AuditorError.Type.INCOMPLETE_ANNOTATION, error_text,
                       self.proto.source.file, self.proto.source.line)
      ]
    else:
      return []

  def check_consistent(self) -> List[AuditorError]:
    """Checks if annotation fields are consistent."""
    CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed
    policy = self.proto.policy

    if policy.cookies_allowed == CookiesAllowed.NO and policy.cookies_store:
      return [
          AuditorError(
              AuditorError.Type.INCONSISTENT_ANNOTATION,
              "Cookies store is specified while cookies are not allowed.",
              self.proto.source.file, self.proto.source.line)
      ]

    if policy.chrome_policy and policy.policy_exception_justification:
      return [
          AuditorError(
              AuditorError.Type.INCONSISTENT_ANNOTATION,
              "Both chrome policies and policy exception justification are "
              "present.", self.proto.source.file, self.proto.source.line)
      ]

    return []


class ExceptionType(Enum):
  """Valid exception types in safe_list.txt."""
  # Ignore all errors (doesn't check the files at all).
  ALL = "all"
  # Ignore missing annotations.
  MISSING = "missing"
  # Ignore direct assignment of annotation value.
  DIRECT_ASSIGNMENT = "direct_assignment"
  # Ignore usages of annotation for tests.
  TEST_ANNOTATION = "test_annotation"
  # Ignore CreateMutableNetworkTrafficAnnotationTag().
  MUTABLE_TAG = "mutable_tag"


# Rules from safe_list.txt, extracted and pre-processed.
IgnoreList = Dict[ExceptionType, List[re.Pattern]]


class FileFilter:
  """Provides the list of files to scan via extractor.py.

  Attributes:
    git_files: The list of files extracted via `git ls-files` (filtered).
    git_file_for_testing: If present, use this .txt file to mock the output of
       `git ls-files`."""

  def __init__(self):
    self.git_files: List[str] = []
    self.git_file_for_testing: Optional[str] = None

  def get_source_files(self, ignore_list: IgnoreList,
                       directory_name: str) -> List[str]:
    """Returns a filtered list of files in the directory_name directory.

    Relevant files:
      - Are tracked by git.
      - Are in a supported programming language (see
        _is_supported_source_file()).
      - Do not match any of the regexen in the ALL category of ignore_list.
      - Are inside the directory_name directory."""
    file_paths = []

    if not self.git_files:
      self.get_files_from_git()

    for f in self.git_files:
      if not f.startswith(directory_name):
        continue
      if (ExceptionType.ALL in ignore_list
          and any(r.match(f) for r in ignore_list[ExceptionType.ALL])):
        continue
      file_paths.append(f)

    return file_paths

  def _is_supported_source_file(self, file_path: str) -> bool:
    """Returns true if file_path looks like a non-test C++/Obj-C++ file."""
    # Check file extension.
    if not re.search(r'\.(cc|mm)$', file_path):
      return False

    # Ignore test files to speed up the tests. They would be only tested when
    # filters are disabled.
    if re.search(r'test\.(cc|mm)$', file_path):
      return False

    return True

  def get_files_from_git(self) -> None:
    """Populates self.git_files with the output of `git ls-files`.

    Only keeps supported source file (per _is_supported_source_file())."""
    # Change directory to source path to access git and check files.
    original_cwd = os.getcwd()
    os.chdir(SRC_DIR)

    if self.git_file_for_testing is not None:
      # Get list of files from git_list.txt (or similar).
      with open(self.git_file_for_testing) as f:
        lines = [l.rstrip() for l in f.readlines()]
    else:
      # Get list of files from git.
      if platform.system() == "Windows":
        command_line = ["git.bat", "ls-files"]
      else:
        command_line = ["git", "ls-files"]
      process = subprocess.run(command_line, capture_output=True)
      lines = process.stdout.decode('utf-8').split('\n')

    self.git_files = [
        f for f in lines if f and self._is_supported_source_file(f)
    ]

    # Now that we're done, undo the chdir().
    os.chdir(original_cwd)


class IdChecker:
  """Performs tests to ensure that annotations have correct ids.

  Attributes:
    reserved_ids: List of IDs that shouldn't be used in code (e.g. test,
        missing, no_traffic_annotation_yet ids).
    deprecated_ids: List of IDs that were used in code before, but shouldn't be
        anymore."""

  def __init__(self, reserved_ids: List[UniqueId],
               deprecated_ids: List[UniqueId]):
    self.reserved_ids = reserved_ids
    self.deprecated_ids = deprecated_ids

    self._annotations: Set[Annotation] = set()

  def check_ids(self, annotations: List[Annotation]) -> List[AuditorError]:
    """Checks annotations for UniqueId-related errors and returns them."""
    self._annotations = set(annotations)
    errors = []

    errors.extend(self._check_ids_format())
    errors.extend(self._check_for_second_ids())
    errors.extend(
        self._check_for_invalid_values(self.reserved_ids,
                                       AuditorError.Type.RESERVED_ID_HASH_CODE))
    errors.extend(
        self._check_for_invalid_values(
            self.deprecated_ids, AuditorError.Type.DEPRECATED_ID_HASH_CODE))
    errors.extend(self._check_for_hash_collisions())
    errors.extend(self._check_for_invalid_repeated_ids())

    return errors

  def _check_ids_format(self) -> List[AuditorError]:
    """Checks if ids only include alphanumeric chars and underscores."""
    errors = []

    for annotation in self._annotations:
      for id, hash_code in annotation.get_ids():
        if not re.match(r"^[0-9a-zA-Z_]*$", id):
          errors.append(
              AuditorError(AuditorError.Type.ID_INVALID_CHARACTER, id,
                           annotation.proto.source.file,
                           annotation.proto.source.line))

    return errors

  def _check_for_second_ids(self) -> List[AuditorError]:
    """Checks if annotation that needs 2 ids, have 2 different ids."""
    errors = []

    for annotation in self._annotations:
      if (annotation.needs_two_ids() and
          (not annotation.second_id or
           annotation.second_id_hash_code == annotation.unique_id_hash_code)):
        errors.append(
            AuditorError(AuditorError.Type.MISSING_SECOND_ID, "",
                         annotation.proto.source.file,
                         annotation.proto.source.line))

    return errors

  def _check_for_invalid_values(self, invalid_ids: List[UniqueId],
                                error_type: AuditorError.Type
                                ) -> List[AuditorError]:
    """Checks that invalid_ids are not used in annotations.

    If found, returns an error with error_type."""
    errors = []

    for annotation in self._annotations:
      for id, hash_code in annotation.get_ids():
        if id in invalid_ids:
          errors.append(
              AuditorError(error_type, id, annotation.proto.source.file,
                           annotation.proto.source.line))

    return errors

  def _check_for_hash_collisions(self) -> List[AuditorError]:
    """Checks that there are no ids with colliding hash values."""
    errors = []
    collisions: Dict[HashCode, UniqueId] = {}

    for annotation in self._annotations:
      for id, hash_code in annotation.get_ids():
        if hash_code not in collisions:
          # If item is loaded from archive, do not keep the second ID for
          # checks. The archive only keeps the hash code, not the second ID
          # itself.
          if (not annotation.is_loaded_from_archive
              or id == annotation.unique_id):
            collisions[hash_code] = id
        else:
          if annotation.is_loaded_from_archive and id == annotation.second_id:
            continue
          if id != collisions[hash_code]:
            errors.append(
                AuditorError(AuditorError.Type.HASH_CODE_COLLISION, id, "", 0,
                             collisions[hash_code]))

    return errors

  def _check_for_invalid_repeated_ids(self) -> List[AuditorError]:
    """Check that there are no invalid repeated ids."""
    errors = []

    first_ids: Dict[HashCode, Annotation] = {}
    second_ids: Dict[HashCode, Annotation] = {}

    # Check if first ids are unique.
    for annotation in self._annotations:
      if annotation.unique_id_hash_code not in first_ids:
        first_ids[annotation.unique_id_hash_code] = annotation
      else:
        errors.append(
            IdChecker._create_repeated_id_error(
                annotation.unique_id, annotation,
                first_ids[annotation.unique_id_hash_code]))

    # If a second id is equal to a first id, the second id should be PARTIAL and
    # the first id should be COMPLETING.
    for annotation in self._annotations:
      if (annotation.needs_two_ids()
          and annotation.second_id_hash_code in first_ids):
        partial = annotation
        completing: Annotation = first_ids[partial.second_id_hash_code]
        if (completing != partial
            and (partial.type != Annotation.Type.PARTIAL
                 or completing.type != Annotation.Type.COMPLETING)):
          errors.append(
              IdChecker._create_repeated_id_error(partial.second_id, partial,
                                                  completing))

    # If two second ids are equal, they should be either PARTIAL or
    # BRANCHED_COMPLETING.
    for annotation in self._annotations:
      if not annotation.needs_two_ids():
        continue
      if annotation.second_id_hash_code not in second_ids:
        second_ids[annotation.second_id_hash_code] = annotation
      else:
        other = second_ids[annotation.second_id_hash_code]
        allowed_types = [
            Annotation.Type.PARTIAL, Annotation.Type.BRANCHED_COMPLETING
        ]
        if (annotation.type not in allowed_types
            or other.type not in allowed_types):
          errors.append(
              self._create_repeated_id_error(annotation.second_id, annotation,
                                             other))

    return errors

  @classmethod
  def _create_repeated_id_error(cls, common_id: UniqueId,
                                annotation1: Annotation,
                                annotation2: Annotation) -> AuditorError:
    """Constructs and returns a REPEATED_ID error."""
    return AuditorError(
        AuditorError.Type.REPEATED_ID,
        "{} in '{}:{}'".format(common_id, annotation1.proto.source.file,
                               annotation1.proto.source.line), "", 0,
        "'{}:{}'".format(annotation2.proto.source.file,
                         annotation2.proto.source.line))


class ArchivedAnnotation(NamedTuple):
  """A record type for annotations.xml entries.

  All values are exactly the same as those stored in annotations.xml, except for
  some type conversions and default values."""

  type: int = -1
  unique_id: UniqueId = UniqueId("")
  unique_id_hash_code: HashCode = HashCode(-1)
  second_id_hash_code: HashCode = HashCode(-1)
  content_hash_code: HashCode = HashCode(-1)

  deprecation_date: str = ""
  os_list: List[str] = []
  added_in_milestone: int = 0

  semantics_fields: List[int] = []
  policy_fields: List[int] = []
  file_path: str = ""


class Auditor:
  #TODO(https://crbug.com/1119417): Filter when given a safelist path.

  def __init__(self):
    self.extracted_annotations: List[Annotation] = []
    self.errors: List[AuditorError] = []

  @staticmethod
  def run_extractor(filter_files: List[str]) -> List[extractor.Annotation]:
    """
    Args:
      filter_files: List[str]
        If this list is empty, parse all .cc files in the repository.

    Returns:
      A list of all network traffic annotation instances found within a list of
      files.
    """
    all_annotations = []
    if filter_files:
      for cc_file in filter_files:
        cc_file = os.path.join(SRC_DIR, cc_file)
        annotations = extractor.extract_annotations(cc_file)
        if annotations:
          all_annotations += annotations
    else:
      #TODO(https://crbug.com/1119417): Parse the entire repository after
      # performing safe_list filtering and etc.
      logger.info("Parsing all valid .cc files in the Chrome repository.")

    return all_annotations

  def parse_extractor_output(self, all_annotations: List[extractor.Annotation]):
    for serialized_annotation in all_annotations:
      annotation = Annotation()
      #TODO(https://crbug.com/1119417): Add all the remaining checks.
      errors = annotation.deserialize(serialized_annotation)
      if not errors:
        self.extracted_annotations.append(annotation)
      else:
        self.errors.extend(errors)

  def check_annotations_contents(self) -> None:
    assert self.extracted_annotations
    for annotation in self.extracted_annotations:
      if annotation.type == Annotation.Type.COMPLETE:
        # Check for completeness.
        errors = annotation.check_complete()
        if not errors:
          errors = annotation.check_consistent()
        self.errors.extend(errors)

        #TODO(https://crbug.com/1119417): Perform the other checks, e.g.
        # consistency.

  def run_all_checks(self) -> None:
    self.check_annotations_contents()


class AuditorUI:
  def __init__(self, build_path, path_filters=[], no_filtering=True):
    self.build_path = build_path
    self.path_filters = path_filters
    self.no_filtering = no_filtering
    #TODO(https://crbug.com/1119417): Should be passed via cmd line.
    self.test_only = True

    self.traffic_annotation = self.import_compiled_proto()
    self.auditor = Auditor()

  def import_compiled_proto(self) -> Any:
    """Global import from function. |self.build_path| is needed to perform
    this import, hence why it's not a top-level import.

    The compiled proto is located ${self.build_path}/pyproto/ and generated
    as a part of compiling Chrome."""
    # Use the build path to import the compiled traffic annotation proto.
    traffic_annotation_proto_path = os.path.join(
        self.build_path, "pyproto/tools/traffic_annotation")
    sys.path.insert(0, traffic_annotation_proto_path)

    try:
      global traffic_annotation_pb2
      global traffic_annotation
      import traffic_annotation_pb2
      # Used for accessing enum constants.
      from traffic_annotation_pb2 import NetworkTrafficAnnotation as \
        traffic_annotation
      return traffic_annotation
    except ImportError as e:
      logger.critical(
        "Failed to import the compiled traffic annotation proto. Make sure "+ \
        "Chrome is built in '{}' before running this script.".format(
          self.build_path))
      raise

  def main(self) -> int:
    if self.no_filtering and self.path_filters:
      logger.warning("The path_filters input is being ignored.")
      self.path_filters = []

    all_annotations = Auditor.run_extractor(self.path_filters)
    self.auditor.parse_extractor_output(all_annotations)

    # Perform checks on successfully extracted annotations, otherwise skip to
    # reporting errors.
    if self.auditor.extracted_annotations:
      self.auditor.run_all_checks()

    # Postprocess errors and dump to stdout.
    if self.auditor.errors:
      print("[Errors]")
      for i, error in enumerate(self.auditor.errors):
        print("  ({})\t{}".format(i + 1, str(error)))
      return 1

    print("traffic annotations are all OK.\n")
    return 0


if __name__ == "__main__":
  args_parser = argparse.ArgumentParser(
      description="Traffic Annotation Auditor: Extracts network traffic"
      " annotations from the repository, audits them for errors and coverage,"
      " produces reports, and updates related files.",
      prog="auditor.py",
      usage="%(prog)s [OPTION] ... [path_filters]")
  args_parser.add_argument("--build-path",
                           help="Path to the build directory.",
                           required=True)
  args_parser.add_argument(
      "--no-filtering",
      action="store_true",
      help="Optional flag asking the tool"
      " to run on the whole repository without text filtering files.")
  args_parser.add_argument(
      "--test-only",
      help="Optional flag to request just running tests and not"
      " updating any file. If not specified,"
      " 'tools/traffic_annotation/summary/annotations.xml' might get updated.",
      action="store_true")
  args_parser.add_argument(
      "--error-resilient",
      help="Optional flag, stating not to return error in"
      " exit code if auditor fails to perform the tests. This flag can be used"
      " for trybots to avoid spamming when tests cannot run.",
      action="store_true")
  args_parser.add_argument("--limit",
                           default=5,
                           help="Limit for the maximum number of returned "
                           " errors. Use 0 for unlimited.")
  args_parser.add_argument("--annotations-file",
                           help="Optional path to a TSV output file with all"
                           " annotations.")
  args_parser.add_argument(
      "path_filters",
      nargs="*",
      help="Optional paths to filter which files the"
      " tool is run on. It can also include deleted files names when auditor is"
      " run on a partial repository. These are ignored if all of the following"
      " are true: Not using --extractor-input, using -no-filtering OR"
      " --all-files, using the python extractor.")

  args = args_parser.parse_args()
  build_path = args.build_path

  print("Starting traffic annotation auditor. This may take a few minutes.")
  auditor_ui = AuditorUI(build_path, args.path_filters, args.no_filtering)

  try:
    sys.exit(auditor_ui.main())
  except Exception as e:
    if args.error_resilient:
      traceback.print_exc()
      sys.exit(0)
    else:
      raise
