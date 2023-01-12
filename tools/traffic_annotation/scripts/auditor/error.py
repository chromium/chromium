# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path
from enum import Enum, auto
from typing import Optional


class ErrorType(Enum):
  # Annotation syntax is not right.
  SYNTAX = auto()
  # Can't create a MutableNetworkTrafficAnnotationTag from anywhere (except
  # in whitelisted files).
  MUTABLE_TAG = auto()
  # Annotation has some missing fields.
  INCOMPLETE_ANNOTATION = auto()
  # A partial of (branched-)completing annotation is not paired with another
  # annotation to be completed.
  INCOMPLETED_ANNOTATION = auto()
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
  # "os_list" is invalid in annotations.xml.
  INVALID_OS = auto()
  # "added_in_milestone" is invalid in annotations.xml.
  INVALID_ADDED_IN = auto()
  # annotations.xml requires an update.
  ANNOTATIONS_XML_UPDATE = auto()
  # grouping.xml requires an update.
  GROUPING_XML_UPDATE = auto()
  # Annotations should be added to grouping.xml.
  ADD_GROUPING_XML = auto()
  # Annotations should be removed from grouping.xml.
  REMOVE_GROUPING_XML = auto()
  # Annotation is missing internal email, user_data
  # or last_reviewed fields
  MISSING_NEW_FIELDS = auto()
  # Annotation should be removed from safe_list.txt
  REMOVE_FROM_SAFE_LIST = auto()
  # User data type should not be unspecified
  INVALID_USER_DATA_TYPE = auto()
  # Date format should be YYYY-mm-dd
  INVALID_DATE_FORMAT = auto()


class AuditorError:
  def __init__(self,
               result_type: ErrorType,
               message: str = "",
               file_path: Optional[Path] = None,
               line: int = 0,
               *extra_details: str):
    self.type = result_type
    self.message = message
    self.file_path = file_path
    self.line = line
    self._details = []

    assert message or result_type in [
        ErrorType.MISSING_TAG_USED, ErrorType.TEST_ANNOTATION,
        ErrorType.NO_ANNOTATION, ErrorType.MISSING_SECOND_ID,
        ErrorType.MUTABLE_TAG, ErrorType.INVALID_OS, ErrorType.INVALID_ADDED_IN,
        ErrorType.INVALID_DATE_FORMAT
    ]

    if message:
      self._details.append(message)
    self._details.extend(extra_details)

  def __str__(self) -> str:
    if self.type == ErrorType.SYNTAX:
      assert self._details
      return ("SYNTAX: Annotation at '{}:{}' has the following syntax"
              " error: {}".format(self.file_path, self.line,
                                  str(self._details[0]).replace("\n", " ")))

    if self.type == ErrorType.MUTABLE_TAG:
      return ("MUTABLE_TAG: Calling CreateMutableNetworkTrafficAnnotationTag() "
              "is not safelisted at '{}:{}'.".format(self.file_path, self.line))

    if self.type == ErrorType.INCOMPLETE_ANNOTATION:
      assert self._details
      return ("INCOMPLETE_ANNOTATION: Annotation at '{}:{}' has the"
              " following missing fields: {}".format(self.file_path, self.line,
                                                     self._details[0]))

    if self.type == ErrorType.INCOMPLETED_ANNOTATION:
      assert self._details
      return ("INCOMPLETED_ANNOTATION: Annotation '{}' is never "
              "completed.".format(self._details[0]))

    if self.type == ErrorType.INCONSISTENT_ANNOTATION:
      assert self._details
      return ("INCONSISTENT_ANNOTATION: Annotation at '{}:{}' has the "
              "following inconsistencies: {}".format(self.file_path, self.line,
                                                     self._details[0]))
    if self.type == ErrorType.MERGE_FAILED:
      assert len(self._details) == 3
      return ("MERGE_FAILED: Annotations '{}' and '{}' cannot be merged due to "
              "the following error(s): {}".format(self._details[1],
                                                  self._details[2],
                                                  self._details[0]))

    if self.type == ErrorType.MISSING_TAG_USED:
      return ("MISSING_TAG_USED: MISSING_TRAFFIC_ANNOTATION tag used in "
              "'{}:{}'.".format(self.file_path, self.line))

    if self.type == ErrorType.TEST_ANNOTATION:
      return ("TEST_ANNOTATION: Annotation for tests used in '{}:{}'.".format(
          self.file_path, self.line))

    if self.type == ErrorType.NO_ANNOTATION:
      return "NO_ANNOTATION: Empty annotation in '{}:{}'.".format(
          self.file_path, self.line)

    if self.type == ErrorType.RESERVED_ID_HASH_CODE:
      assert self._details
      return ("RESERVED_ID_HASH_CODE: Id '{}' in '{}:{}' has a hash code equal "
              "to a reserved word and should be changed.".format(
                  self._details[0], self.file_path, self.line))

    if self.type == ErrorType.HASH_CODE_COLLISION:
      assert len(self._details) == 2
      return ("HASH_CODE_COLLISION: The following annotations have colliding "
              "hash codes and should be updated: '{}', '{}'.".format(
                  self._details[0], self._details[1]))

    if self.type == ErrorType.REPEATED_ID:
      assert len(self._details) == 2
      return ("REPEATED_ID: The following annotations have equal ids and "
              "should be updated: {}, {}.".format(self._details[0],
                                                  self._details[1]))

    if self.type == ErrorType.ID_INVALID_CHARACTER:
      assert self._details
      return ("ID_INVALID_CHARACTER: Id '{}' in '{}:{}' contains an invalid "
              "character.".format(self._details[0], self.file_path, self.line))

    if self.type == ErrorType.MISSING_SECOND_ID:
      return ("MISSING_SECOND_ID: Second id of annotation at '{}:{}' should be "
              "updated, as it has the same hash code as the first one.".format(
                  self.file_path, self.line))

    if self.type == ErrorType.INVALID_OS:
      assert len(self._details) == 2
      return ("INVALID_OS: Invalid OS '{}' in annotation '{}' at {}.".format(
          self._details[0], self._details[1], self.file_path))

    if self.type == ErrorType.INVALID_ADDED_IN:
      assert len(self._details) == 2
      return ("INVALID_ADDED_IN: Invalid or missing added_in_milestone '{}' in "
              "annotation '{}' at {}.".format(self._details[0],
                                              self._details[1], self.file_path))

    if self.type == ErrorType.ADD_GROUPING_XML:
      assert self._details
      return ("ADD_GROUPING_XML: The following annotations should be added "
              "to an existing group in "
              "tools/traffic_annotation/summary/grouping.xml: {}.".format(
                  self._details[0]))

    if self.type == ErrorType.REMOVE_GROUPING_XML:
      assert self._details
      return ("REMOVE_GROUPING_XML: The following annotations are not needed "
              "in tools/traffic_annotation/summary/grouping.xml, and should be "
              "removed: {}.".format(self._details[0]))

    if self.type == ErrorType.ANNOTATIONS_XML_UPDATE:
      assert self._details
      return (
          "'tools/traffic_annotation/summary/annotations.xml' requires update. "
          "It is recommended to run the Traffic Annotation Auditor locally to "
          "do the updates automatically (please refer to tools/"
          "traffic_annotation/scripts/auditor/README.md), but you can also "
          "apply the following edit(s) to do it manually:\n{}".format(
              self._details[0]))

    if self.type == ErrorType.GROUPING_XML_UPDATE:
      assert self._details
      return (
          "'tools/traffic_annotation/summary/grouping.xml' requires update. "
          "It is recommended to run the Traffic Annotation Auditor locally to "
          "do the updates automatically (please refer to tools/"
          "traffic_annotation/scripts/auditor/README.md), but you can also "
          "apply the following edit(s) to do it manually:\n{}".format(
              self._details[0]))

    if self.type == ErrorType.MISSING_NEW_FIELDS:
      assert self._details
      return ("MISSING_NEW_FIELDS: Annotation at '{}:{}' {}".format(
          self.file_path, self.line, self._details[0]))
    if self.type == ErrorType.REMOVE_FROM_SAFE_LIST:
      assert self._details
      return ("REMOVE_FROM_SAFE_LIST: {}. Remove {} from safe_list.txt".format(
          self._details[0], self.file_path))

    if self.type == ErrorType.INVALID_USER_DATA_TYPE:
      assert self._details
      return (
          "Invalid value of user_data::type: {} in annotation at {}:{}".format(
              self._details[0], self.file_path, self.line))

    if self.type == ErrorType.INVALID_DATE_FORMAT:
      assert self._details
      return ("Date format should be {} in annotation at {}:{}".format(
          self._details[0], self.file_path, self.line))

    raise NotImplementedError("Unimplemented ErrorType: {}".format(
        self.type.name))

  def __repr__(self) -> str:
    return "AuditorError(\"{}\")".format(str(self))
