#!/usr/bin/env vpython
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import copy
import logging
import argparse
import traceback
from aenum import Enum, auto
from google.protobuf import text_format

import extractor


# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
SRC_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "../../.."))

logging.basicConfig(
  level=logging.INFO,
  format="%(filename)s:%(funcName)s:%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)


def iterative_hash(s):
  """Compute the has code of the given string as in:
  net/traffic_annotation/network_traffic_annotation.h

  Args:
    s: str
      The seed, e.g. unique id of traffic annotation.
  Returns: int
    A hash code.
  """
  return reduce(lambda acc, c: (acc*31 + ord(c)) % 138003713, s, 0)


class AuditorError(Exception):
  class Type(Enum):
    ERROR_FATAL = auto()                # A fatal error that should stop the
                                        # process.
    ERROR_SYNTAX = auto()               # Annotation syntax is not right.
    ERROR_INCOMPLETE_ANNOTATION = auto()   # Annotation has some missing fields.
    ERROR_MUTABLE_TAG = auto()          # Can't create a
                                        # |MutableNetworkTrafficAnnotationTag|
                                        # from anywhere (except whitelisted
                                        # files).
    ERROR_NO_ANNOTATION = auto()        # A function is called with
                                        # NO_ANNOTATION tag. Deprecated as is
                                        # now undefined on supported platforms.
    ERROR_MISSING_ANNOTATION = auto()   # A function that requires annotation is
                                        # is not annotated.

  def __init__(self, result_type, message="", file_path="", line=0):
    self.type = result_type
    self.message = message
    self.file_path = file_path
    self.line = line
    self._details = []

    assert message or result_type in [AuditorError.Type.ERROR_NO_ANNOTATION]

    if message:
      self._details.append(message)

  def __str__(self):
    #TODO(https://crbug.com/1119417): Add all the possible errors and their
    # explanations here.
    if self.type == AuditorError.Type.ERROR_SYNTAX:
      assert self._details
      return "ERROR_SYNTAX: Annotation at '{}:{}' has the following syntax" \
        " error: {}".format(
        self.file_path, self.line, str(self._details[0]).replace("\n", " "))

    if self.type == AuditorError.Type.ERROR_INCOMPLETE_ANNOTATION:
      assert self._details
      return "ERROR_INCOMPLETE_ANNOTATION: Annotation at '{}:{}' has the" \
        " following missing fields: {}".format(
          self.file_path, self.line, self._details[0])

    if self.type == AuditorError.Type.ERROR_FATAL:
      assert self._details
      return "ERROR_FATAL: Annotation at '{}:{}' has a fatal error: {}".format(
        self.file_path, self.line, self._details[0])


class AnnotationInstance(object):
  class Type(Enum):
    ANNOTATION_COMPLETE = "Definition"
    ANNOTATION_PARTIAL = "Partial"
    ANNOTATION_COMPLETING = "Completing"
    ANNOTATION_BRANCHED_COMPLETING = "BranchedCompleting"

  def __init__(self):
    self.proto = traffic_annotation_pb2.NetworkTrafficAnnotation()
    self.type = AnnotationInstance.Type.ANNOTATION_COMPLETING
    self.unique_id_hash_code = -1
    self.second_id = -1
    self.second_id_hash_code = -1

    self.archived_content_hash_code = -1
    self.is_loaded_from_archive = False

  def get_content_hash_code(self):
    #TODO(https://crbug.com/1119417): Address ASCII issue, where it reports an
    # incorrect content hash code when the proto's contents contain a non-ascii
    # character.
    if self.is_loaded_from_archive:
      return self.archived_content_hash_code

    source_free_proto = copy.deepcopy(self.proto)
    source_free_proto.ClearField("source")
    source_free_proto = text_format.MessageToString(
      source_free_proto, as_utf8=True)
    return TrafficAnnotationAuditor.compute_hash_value(source_free_proto)

  def deserialize(self, serialized_annotation):
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
    self.unique_id_hash_code = TrafficAnnotationAuditor.compute_hash_value(
      unique_id)
    self.second_id = serialized_annotation.extra_id
    self.second_id_hash_code = TrafficAnnotationAuditor.compute_hash_value(
      self.second_id)

    try:
      text_format.Parse(serialized_annotation.text, self.proto)
    except Exception as e:
      raise AuditorError(
        AuditorError.Type.ERROR_SYNTAX, e, file_path, line_number)

    self.proto.source.file = file_path
    self.proto.source.line = line_number
    self.proto.unique_id = unique_id

  def check_complete(self):
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
      raise AuditorError(
        AuditorError.Type.ERROR_INCOMPLETE_ANNOTATION,
        error_text, self.proto.source.file, self.proto.source.line)


class TrafficAnnotationAuditor(object):
  #TODO(https://crbug.com/1119417): Filter when given a safelist path.

  def __init__(self):
    self.extracted_annotations = []
    self.errors = []

  @staticmethod
  def compute_hash_value(text):
    """Computes the hash value of given text."""
    return iterative_hash(text) if text else -1

  @staticmethod
  def run_extractor(filter_files):
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

  def parse_extractor_output(self, all_annotations):
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

  def check_annotations_contents(self):
    assert self.extracted_annotations
    for annotation in self.extracted_annotations:
      if annotation.type == AnnotationInstance.Type.ANNOTATION_COMPLETE:
        # Check for completeness.
        try:
          annotation.check_complete()
        except AuditorError as e:
          self.errors.append(e)

        #TODO(https://crbug.com/1119417): Perform the other checks, e.g.
        # consistency.

  def run_all_checks(self):
    self.check_annotations_contents()


class TrafficAnnotationAuditorUI(object):
  def __init__(self, build_path, path_filters=[], no_filtering=True):
    self.build_path = build_path
    self.path_filters = path_filters
    self.no_filtering = no_filtering
    #TODO(https://crbug.com/1119417): Should be passed via cmd line.
    self.test_only = True

    self.import_compiled_proto()
    self.auditor = TrafficAnnotationAuditor()

  def import_compiled_proto(self):
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

  def main(self):
    if self.no_filtering and self.path_filters:
      logger.warning("The path_filters input is being ignored.")
      self.path_filters = []

    all_annotations = TrafficAnnotationAuditor.run_extractor(self.path_filters)
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
  auditor_ui = TrafficAnnotationAuditorUI(
    build_path, args.path_filters, args.no_filtering)

  try:
    sys.exit(auditor_ui.main())
  except Exception as e:
    if args.error_resilient:
      traceback.print_exc()
      sys.exit(0)
    else:
      raise
