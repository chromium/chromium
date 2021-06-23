#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import copy
import logging
import os
import sys
import traceback

from enum import Enum, auto
from functools import reduce
from google.protobuf import text_format
from typing import NewType, TYPE_CHECKING, List, Dict

import extractor

if TYPE_CHECKING:
  # For the `mypy` type checker, a hardcoded import that is never used when
  # actually running. The real import is in AuditorUI.import_proto()
  #
  # TODO(nicolaso): Add instructions for running mypy.
  import traffic_annotation_pb2
  from traffic_annotation_pb2 import NetworkTrafficAnnotation as \
      traffic_annotation

UniqueId = NewType('UniqueId', str)
HashCode = NewType('HashCode', int)

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))

# Reserved annotation unique IDs that should only be used in untracked files
# (e.g., test files or files that aren't compiled on this platform).
RESERVED_IDS = ["test", "test_partial", "missing"]

# Host platforms that support running auditor.py.
SUPPORTED_PLATFORMS = ["linux", "windows"]

logging.basicConfig(
    level=logging.INFO,
    format="%(filename)s:%(lineno)d:%(funcName)s:%(levelname)s: %(message)s")
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


class AuditorError(Exception):
  class Type(Enum):
    # Annotation syntax is not right.
    SYNTAX = auto()
    # Annotation has some missing fields.
    INCOMPLETE_ANNOTATION = auto()
    # Can't create a |MutableNetworkTrafficAnnotationTag| from anywhere (except
    # whitelisted files).
    MUTABLE_TAG = auto()
    # A function is called with NO_ANNOTATION tag. Deprecated as is now
    # undefined on supported platforms.
    NO_ANNOTATION = auto()
    # A function that requires annotation is is not annotated.
    MISSING_ANNOTATION = auto()
    # A function uses the "test" or "test_partial" annotation outside of a test
    # file.
    TEST_ANNOTATION = auto()

  def __init__(self, result_type, message="", file_path="", line=0) -> None:
    self.type = result_type
    self.message = message
    self.file_path = file_path
    self.line = line
    self._details = []

    assert message or result_type in [AuditorError.Type.NO_ANNOTATION]

    if message:
      self._details.append(message)

  def __str__(self) -> str:
    #TODO(https://crbug.com/1119417): Add all the possible errors and their
    # explanations here.
    if self.type == AuditorError.Type.SYNTAX:
      assert self._details
      return "SYNTAX: Annotation at '{}:{}' has the following syntax" \
        " error: {}".format(
        self.file_path, self.line, str(self._details[0]).replace("\n", " "))

    if self.type == AuditorError.Type.INCOMPLETE_ANNOTATION:
      assert self._details
      return "INCOMPLETE_ANNOTATION: Annotation at '{}:{}' has the" \
        " following missing fields: {}".format(
          self.file_path, self.line, self._details[0])

    return self.type.name


class AnnotationInstance(object):
  class Type(Enum):
    COMPLETE = "Definition"
    PARTIAL = "Partial"
    COMPLETING = "Completing"
    BRANCHED_COMPLETING = "BranchedCompleting"

  def __init__(self):
    self.proto = traffic_annotation_pb2.NetworkTrafficAnnotation()
    self.type = AnnotationInstance.Type.COMPLETING
    self.unique_id_hash_code: HashCode = -1
    self.second_id: UniqueId = -1
    self.second_id_hash_code: HashCode = -1

    self.archived_content_hash_code: HashCode = -1
    self.is_loaded_from_archive = False

  def get_content_hash_code(self) -> HashCode:
    #TODO(https://crbug.com/1119417): Address ASCII issue, where it reports an
    # incorrect content hash code when the proto's contents contain a non-ascii
    # character.
    if self.is_loaded_from_archive:
      return self.archived_content_hash_code

    source_free_proto = copy.deepcopy(self.proto)
    source_free_proto.ClearField("source")
    source_free_proto = text_format.MessageToString(
      source_free_proto, as_utf8=True)
    return Auditor.compute_hash_value(source_free_proto)

  def deserialize(self, serialized_annotation: extractor.Annotation):
    """Deserializes an instance from extractor.Annotation.

    Args:
      serialized_annotation: extractor.Annotation
    """
    assert len(serialized_annotation.text) > 6, \
      "Not enough lines to deserialize annotation text."

    file_path = os.path.relpath(serialized_annotation.file_path, SRC_DIR)
    line_number = serialized_annotation.line_number
    self.type = AnnotationInstance.Type(serialized_annotation.type_name)

    unique_id = serialized_annotation.unique_id
    self.unique_id_hash_code = Auditor.compute_hash_value(unique_id)
    self.second_id = serialized_annotation.extra_id
    self.second_id_hash_code = Auditor.compute_hash_value(self.second_id)

    try:
      text_format.Parse(serialized_annotation.text, self.proto)
    except Exception as e:
      raise AuditorError(AuditorError.Type.SYNTAX, e, file_path, line_number)

    self.proto.source.file = file_path
    self.proto.source.line = line_number
    self.proto.unique_id = unique_id

  def check_complete(self) -> None:
    """Checks if an annotation has all required fields."""
    unspecifieds = []
    # Check semantic fields.
    semantics = self.proto.semantics
    semantics_fields = ["sender", "description", "trigger", "data"]
    for field in semantics_fields:
      if not getattr(semantics, field):
        unspecifieds.append(field)

    # Check policy fields.
    policy = self.proto.policy
    # cookies_allowed must be specified.
    cookies_allowed = getattr(policy, "cookies_allowed")
    if cookies_allowed == \
      traffic_annotation.TrafficPolicy.CookiesAllowed.UNSPECIFIED:
      unspecifieds.append("cookies_allowed")

    # If cookies_store is not provided, ignore if 'cookies_allowed = NO' is
    # in the list.
    cookies_store = getattr(policy, "cookies_store")
    if not cookies_store and cookies_allowed == \
      traffic_annotation.TrafficPolicy.CookiesAllowed.YES:
      unspecifieds.append("cookies_store")

    # If either of 'chrome_policy' or 'policy_exception_justification' are
    # available, ignore not having the other one.
    chrome_policy = getattr(policy, "chrome_policy")
    policy_exception = getattr(policy, "policy_exception_justification")
    if not chrome_policy and not policy_exception:
      unspecifieds.append("chrome_policy")
      unspecifieds.append("policy_exception_justification")

    if unspecifieds:
      error_text = ", ".join(unspecifieds)
      raise AuditorError(AuditorError.Type.INCOMPLETE_ANNOTATION, error_text,
                         self.proto.source.file, self.proto.source.line)


class Auditor:
  #TODO(https://crbug.com/1119417): Filter when given a safelist path.

  def __init__(self):
    self.extracted_annotations: List[AnnotationInstance] = []
    self.errors: List[AuditorError] = []

  @staticmethod
  def compute_hash_value(text: str) -> HashCode:
    """Computes the hash value of given text."""
    return iterative_hash(text) if text else HashCode(-1)

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
    """
    Args:
      all_annations: List[extractor.Annotation]
    """
    for serialized_annotation in all_annotations:
      annotation = AnnotationInstance()
      try:
        #TODO(https://crbug.com/1119417): Add all the remaining checks.
        annotation.deserialize(serialized_annotation)
        self.extracted_annotations.append(annotation)
      except AuditorError as e:
        self.errors.append(e)

  def check_annotations_contents(self) -> None:
    assert self.extracted_annotations
    for annotation in self.extracted_annotations:
      if annotation.type == AnnotationInstance.Type.COMPLETE:
        # Check for completeness.
        try:
          annotation.check_complete()
        except AuditorError as e:
          self.errors.append(e)

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

    self.import_compiled_proto()
    self.auditor = Auditor()

  def import_compiled_proto(self) -> None:
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
        print("  ({})\t{}".format(i+1, str(error)))
      return 1

    print("traffic annotations are all OK.\n")
    return 0


if __name__ == "__main__":
  args_parser = argparse.ArgumentParser(
    description="Traffic Annotation Auditor: Extracts network traffic"
    " annotations from the repository, audits them for errors and coverage,"
    " produces reports, and updates related files.", prog="auditor.py",
    usage="%(prog)s [OPTION] ... [path_filters]")
  args_parser.add_argument(
    "--build-path", help="Path to the build directory.", required=True)
  args_parser.add_argument(
    "--no-filtering", action="store_true", help="Optional flag asking the tool"
    " to run on the whole repository without text filtering files.")
  args_parser.add_argument(
    "--test-only", help="Optional flag to request just running tests and not"
    " updating any file. If not specified,"
    " 'tools/traffic_annotation/summary/annotations.xml' might get updated.",
    action="store_true")
  args_parser.add_argument(
    "--error-resilient", help="Optional flag, stating not to return error in"
    " exit code if auditor fails to perform the tests. This flag can be used"
    " for trybots to avoid spamming when tests cannot run.",
    action="store_true")
  args_parser.add_argument(
    "--limit", default=5, help="Limit for the maximum number of returned "
    " errors. Use 0 for unlimited.")
  args_parser.add_argument(
    "--annotations-file", help="Optional path to a TSV output file with all"
    " annotations.")
  args_parser.add_argument(
    "path_filters", nargs="*", help="Optional paths to filter which files the"
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
