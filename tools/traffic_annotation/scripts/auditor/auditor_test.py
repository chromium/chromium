#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Tests for the Traffic Annotation Auditor.
"""

import argparse
import itertools
import os
import platform
import sys
import tempfile
import unittest

from typing import cast, Tuple

from auditor import *
from error import *
from util import *

# Path to the test_data/ dir.
TEST_DATA_DIR = SCRIPT_DIR.parent / "test_data"


class AuditorTest(unittest.TestCase):
  def setUp(self):
    build_path = TEST_DATA_DIR / "out" / "Debug"

    unittest.TestCase.setUp(self)

    path_filters = [
        (TEST_DATA_DIR /
         "test_sample_annotations.cc").relative_to(SRC_DIR).as_posix(),
        (TEST_DATA_DIR /
         "missing_new_field_sample_data/test_new_field_safelisted.cc"
         ).relative_to(SRC_DIR).as_posix(),
        (TEST_DATA_DIR /
         "missing_new_field_sample_data/sample_new_field_not_safelisted.cc"
         ).relative_to(SRC_DIR).as_posix()
    ]
    self.auditor_ui = AuditorUI(build_path,
                                path_filters,
                                no_filtering=False,
                                test_only=True)
    self.auditor = self.auditor_ui.auditor
    self.auditor.file_filter.git_file_for_testing = (TEST_DATA_DIR /
                                                     "git_list.txt")

    all_annotations = self.auditor.run_extractor(self.auditor_ui.build_path,
                                                 self.auditor_ui.path_filters,
                                                 skip_compdb=True)

    self.sample_annotations = {}
    for annotation in all_annotations:
      self.sample_annotations[annotation.unique_id] = annotation

    # Expose the traffic_annotation_pb2.py import from auditor.py.
    global traffic_annotation
    traffic_annotation = self.auditor_ui.traffic_annotation

  def deserialize(self,
                  file_name: str) -> Tuple[Annotation, List[AuditorError]]:
    file_path = TEST_DATA_DIR / "extractor_outputs" / file_name
    lines = file_path.read_text(encoding="utf-8").splitlines()

    annotation = Annotation()
    language = extractor.LANGUAGE_MAPPING[Path(lines[0]).suffix]
    type_name = extractor.AnnotationType(lines[2])
    extracted_annotation = extractor.Annotation(language=language,
                                                file_path=lines[0],
                                                line_number=int(lines[1]),
                                                type_name=type_name,
                                                unique_id=lines[3],
                                                extra_id=lines[4],
                                                text="\n".join(lines[5:]))
    errors = annotation.deserialize(extracted_annotation)
    return annotation, errors

  def create_annotation_sample(self,
                               type=Annotation.Type.COMPLETE,
                               unique_id=0,
                               second_id=0) -> Annotation:
    if not unique_id:
      instance, errors = self.deserialize("good_complete_annotation.txt")
      assert not errors
      return instance
    instance = Annotation()
    instance.type = type
    instance.unique_id = UniqueId("S{}".format(unique_id))
    if second_id:
      instance.second_id = UniqueId("S{}".format(second_id))
    else:
      instance.second_id = UniqueId("")
    return instance

  def run_id_checker(self, annotation: Annotation) -> List[AuditorError]:
    id_checker = IdChecker(RESERVED_IDS)
    return id_checker.check_ids([annotation])

  def test_iterative_hash(self):
    """Tests that the hash computation is identical to the C++ version."""
    # Reference values obtained from recursive_hash() in C++.
    self.assertEqual(3556498, iterative_hash("test"))
    self.assertEqual(10236504, iterative_hash("unique_id"))
    self.assertEqual(70581310, iterative_hash("123_id"))
    self.assertEqual(69491511, iterative_hash("ID123"))
    self.assertEqual(
        98652091,
        iterative_hash(
            "a_unique_looooooooooooooooooooooooooooooooooooooooooooooooooooooong"
            "_id"))
    self.assertEqual(124751853, iterative_hash("bébé"))

  def test_get_files_from_git(self):
    """Tests that FileFilter.get_files_from_git() returns correct files given
    a mock git_list.txt file. It also inherently checks
    FileFilter._is_supported_source_file()."""
    filter = FileFilter([".cc", ".mm"])
    filter.git_file_for_testing = TEST_DATA_DIR / "git_list.txt"
    filter.get_files_from_git()

    relevant_files = [
        "tools/traffic_annotation/scripts/test_data/"
        "objective_cpp.mm", "tools/traffic_annotation/scripts/test_data/"
        "test_sample_annotations.cc",
        "tools/traffic_annotation/scripts/test_data/"
        "missing_new_field_sample_data/test_new_field_safelisted.cc",
        "tools/traffic_annotation/scripts/test_data/"
        "missing_new_field_sample_data/sample_new_field_not_safelisted.cc"
    ]
    self.assertCountEqual([Path(f) for f in relevant_files], filter.git_files)

  def test_get_source_files(self):
    """Tests that FileFilter.get_source_files() gives the correct list of
    files, given a mock git_list.txt file."""
    filter = FileFilter([".cc", ".mm"])
    filter.git_file_for_testing = TEST_DATA_DIR / "git_list.txt"
    filter.get_files_from_git()

    # Check if all files are returned with no ignore list and directory.
    ignore_list = {}
    self.assertCountEqual(filter.git_files,
                          filter.get_source_files(ignore_list, ""))

    # Check if a file is ignored when added to the ignore list.
    ignore_list = {
        ExceptionType.ALL: [re.compile(filter.git_files[0].as_posix())]
    }
    self.assertCountEqual(
        set(filter.git_files) - set(filter.git_files[:1]),
        filter.get_source_files(ignore_list, ""))

    # Check if files are filtered based on given directory.
    ignore_list = {}
    self.assertCountEqual(
        filter.git_files,
        filter.get_source_files(ignore_list, "tools/traffic_annotation"))
    self.assertEqual([], filter.get_source_files(ignore_list, "content"))

  def test_is_safelisted(self):
    """Tests if Auditor._is_safe_listed() works as expected. Inherently checks
    Auditor.load_safe_list() as well."""
    auditor = Auditor(get_current_platform())  # use the real safe_list.txt
    for t in ExceptionType:
      # Anything in /tools directory is safelisted for all types.
      self.assertTrue(auditor._is_safe_listed(Path("tools/something.cc"), t))
      self.assertTrue(
          auditor._is_safe_listed(Path("tools/somewhere/something.mm"), t))

      # Anything in a general folder is not safelisted for any type.
      self.assertFalse(auditor._is_safe_listed(Path("something.cc"), t))
      self.assertFalse(auditor._is_safe_listed(Path("content/something.mm"), t))

    # Files defining missing annotation functions in net/ are exception of
    # 'missing' type.
    self.assertFalse(
        auditor._is_safe_listed(Path("net/url_request/url_fetcher.cc"),
                                ExceptionType.MISSING))
    self.assertTrue(
        auditor._is_safe_listed(Path("net/url_request/url_request_context.cc"),
                                ExceptionType.MISSING))

    # Files with the word "test" in their path can have the "test" annotation.
    self.assertFalse(
        auditor._is_safe_listed(Path("net/url_request/url_fetcher.cc"),
                                ExceptionType.TEST_ANNOTATION))
    self.assertTrue(
        auditor._is_safe_listed(Path("chrome/browser/test_something.cc"),
                                ExceptionType.TEST_ANNOTATION))
    self.assertTrue(
        auditor._is_safe_listed(Path("test/send_something.cc"),
                                ExceptionType.TEST_ANNOTATION))

  def test_annotation_deserialization(self) -> None:
    test_cases = [
        ("good_complete_annotation.txt", None, Annotation.Type.COMPLETE),
        ("good_branched_completing_annotation.txt", None,
         Annotation.Type.BRANCHED_COMPLETING),
        ("good_completing_annotation.txt", None, Annotation.Type.COMPLETING),
        ("good_partial_annotation.txt", None, Annotation.Type.PARTIAL),
        ("good_test_annotation.txt", ErrorType.TEST_ANNOTATION, None),
        ("missing_annotation.txt", ErrorType.MISSING_TAG_USED, None),
        ("good_no_annotation.txt", ErrorType.NO_ANNOTATION, None),
        ("bad_syntax_annotation1.txt", ErrorType.SYNTAX, None),
        ("bad_syntax_annotation2.txt", ErrorType.SYNTAX, None),
        ("bad_syntax_annotation3.txt", ErrorType.SYNTAX, None),
        ("bad_syntax_annotation4.txt", ErrorType.SYNTAX, None),
        ("bad_syntax_annotation5.txt", ErrorType.SYNTAX, None),
        ("bad_syntax_annotation6.txt", ErrorType.SYNTAX, None),
        # "fatal" means a Python exception gets raised.
        ("fatal_annotation1.txt", "fatal", None),
        ("fatal_annotation2.txt", "fatal", None),
        ("fatal_annotation3.txt", "fatal", None),
    ]

    for test_case in test_cases:
      logger.debug("Testing: {}".format(test_case))

      (file_name, error_type, annotation_type) = test_case

      if error_type == "fatal":
        # Should raise a Python exception.
        with self.assertRaises(Exception):
          annotation, errors = self.deserialize(file_name)
      elif error_type:
        # Should raise an AuditorError of the specified type.
        annotation, errors = self.deserialize(file_name)
        self.assertEqual(1, len(errors))
        self.assertEqual(error_type, errors[0].type)
      else:
        # Should not raise an error.
        annotation, errors = self.deserialize(file_name)
        self.assertEqual([], errors)
        self.assertEqual(annotation_type, annotation.type, file_name)

      # Check contents for one complete sample.
      if file_name != "good_complete_annotation.txt":
        continue

      self.assertEqual(annotation.unique_id,
                       "supervised_user_refresh_token_fetcher")
      self.assertEqual(
          annotation.file,
          Path("chrome/browser/supervised_user/legacy/"
               "supervised_user_refresh_token_fetcher.cc"))
      self.assertEqual(annotation.line, 166)
      self.assertEqual(annotation.proto.semantics.sender, "Supervised Users")
      self.assertEqual(annotation.proto.policy.cookies_allowed, 1)

  def test_get_reserved_ids_coverage(self):
    """Tests if RESERVED_IDS has all known ids."""
    self.assertEqual(["test", "test_partial", "missing", "undefined"],
                     RESERVED_IDS)

  def test_reserved_ids_usage_detection(self):
    """Tests if use of reserved ids are detected."""
    for reserved_id in RESERVED_IDS:
      annotation = Annotation()
      annotation.type = Annotation.Type.COMPLETE
      annotation.unique_id = reserved_id
      errors = self.run_id_checker(annotation)
      self.assertEqual(1, len(errors))
      self.assertEqual(ErrorType.RESERVED_ID_HASH_CODE, errors[0].type)

      annotation.type = Annotation.Type.PARTIAL
      annotation.unique_id = "nonempty"
      annotation.second_id = reserved_id
      errors = self.run_id_checker(annotation)
      self.assertEqual(1, len(errors))
      self.assertEqual(ErrorType.RESERVED_ID_HASH_CODE, errors[0].type)

  def test_repeated_ids_detection(self):
    """Tests if use of repeated ids are detected."""
    id_checker = IdChecker([])

    # Check if several different hash codes result in no error.
    annotations = [
        self.create_annotation_sample(unique_id=i) for i in range(10)
    ]
    errors = id_checker.check_ids(annotations)
    self.assertEqual([], errors)

    # Check if repeating the same hash codes results in errors.
    annotations = [
        self.create_annotation_sample(unique_id=i // 2) for i in range(20)
    ]
    errors = id_checker.check_ids(annotations)
    self.assertCountEqual([ErrorType.REPEATED_ID] * 10,
                          [e.type for e in errors])

  def test_similar_unique_and_second_ids_detection(self):
    """Tests if having the same unique id and second id is detected."""
    for t in Annotation.Type:
      annotation = Annotation()
      annotation.type = t
      annotation.unique_id = "the_same"
      annotation.second_id = "the_same"
      errors = self.run_id_checker(annotation)
      if annotation.needs_two_ids():
        self.assertEqual(1, len(errors), t)
      else:
        self.assertEqual(0, len(errors), t)

  def test_duplicate_ids_detection(self):
    """Tests unique id and second id collision cases."""
    T = Annotation.Type
    annotation_types = list(T)
    for type1, type2 in itertools.product(*[list(T)] * 2):
      if annotation_types.index(type2) < annotation_types.index(type1):
        continue

      for id1, id2, id3, id4 in \
          itertools.product(*[range(1, 5) for i in range(4)]):
        logger.debug("Testing ({}, {}, {}, {}, {}, {})".format(
            type1, type2, id1, id2, id3, id4))

        annotation1 = Annotation()
        annotation1.type = type1
        annotation1.unique_id = str(id1)
        annotation1.second_id = str(id2)

        annotation2 = Annotation()
        annotation2.type = type2
        annotation2.unique_id = str(id3)
        annotation2.second_id = str(id4)

        id_checker = IdChecker([])
        errors = id_checker.check_ids([annotation1, annotation2])

        first_needs_two = annotation1.needs_two_ids()
        second_needs_two = annotation2.needs_two_ids()

        unique_ids = set(id for a in [annotation1, annotation2]
                         for id in a.get_ids())

        if first_needs_two and second_needs_two:
          # If both need 2 ids, either the 4 ids should be different, or the
          # second ids should be equal and both annotations should be of types
          # partial/branched-completing.
          if len(unique_ids) == 4:
            self.assertFalse(errors)
          elif len(unique_ids) == 3:
            acceptable = (id2 == id4
                          and type1 in [T.PARTIAL, T.BRANCHED_COMPLETING]
                          and type2 in [T.PARTIAL, T.BRANCHED_COMPLETING])
            self.assertEqual(not acceptable, bool(errors))
          else:
            self.assertTrue(errors)
        elif first_needs_two and not second_needs_two:
          # If just the first one needs two ids, then either the 3 ids should be
          # different or the first annotation would be partial and the second
          # completing, with one common id.
          if len(unique_ids) == 3:
            self.assertFalse(errors)
          elif len(unique_ids) == 2:
            acceptable = (id2 == id3 and type1 == T.PARTIAL
                          and type2 == T.COMPLETING)
            self.assertEqual(not acceptable, bool(errors))
          else:
            self.assertTrue(errors)
        elif not first_needs_two and second_needs_two:
          # Can only be valid if all 3 are different.
          self.assertEqual(len(unique_ids) != 3, bool(errors))
        else:
          # If none requires 2 ids, it can only be valid if ids are different.
          self.assertEqual(len(unique_ids) != 2, bool(errors))

  def test_check_ids_format(self):
    """Tests if IDs' format is correctly checked."""
    test_cases = [
        ("ID1", True),
        ("id2", True),
        ("Id_3", True),
        ("ID?4", False),
        ("ID:5", False),
        ("ID>>6", False),
    ]

    annotation = self.create_annotation_sample()
    for test_case in test_cases:
      # Set type to complete to require just unique ID.
      annotation.type = Annotation.Type.COMPLETE
      annotation.unique_id = test_case[0]
      errors = self.run_id_checker(annotation)
      self.assertEqual(not test_case[1], bool(errors), test_case[0])

      # Set type to partial to require both ids.
      annotation.type = Annotation.Type.PARTIAL
      annotation.unique_id = "Something_Good"
      annotation.second_id = test_case[0]
      self.run_id_checker(annotation)
      self.assertEqual(not test_case[1], bool(errors), test_case[0])

    # Test all cases together.
    annotations = []

    false_sample_count = 0
    for test_case in test_cases:
      annotation = self.create_annotation_sample()
      annotation.type = Annotation.Type.COMPLETE
      annotation.unique_id = test_case[0]
      annotations.append(annotation)
      if not test_case[1]:
        false_sample_count += 1

    id_checker = IdChecker([])
    errors = id_checker.check_ids(annotations)
    self.assertEqual(false_sample_count, len(errors))

  def test_check_complete_annotations(self):
    """Tests if Auditor.check_annotation_contents() works as expected for
    COMPLETE annotations. It also inherently checks
    Auditor.is_annotation_complete(), Auditor.is_annotation_consistent(), and
    Auditor.is_in_grouping_xml()."""
    annotations = []
    expected_errors_count = 0

    Destination = traffic_annotation.TrafficSemantics.Destination
    CookiesAllowed = traffic_annotation.TrafficPolicy.CookiesAllowed

    test_no = 0
    while True:
      annotation = self.create_annotation_sample()
      annotation.unique_id = "foobar_policy_fetcher{}".format(test_no)
      test_description = ""
      expect_error = True
      logger.info(
          "test_check_complete_annotations test number {}".format(test_no))
      if test_no == 0:
        test_description = "All fields OK."
        expect_error = False
      elif test_no == 1:
        test_description = "Missing semantics::sender"
        annotation.proto.semantics.sender = ""
      elif test_no == 2:
        test_description = "Missing semantics::description"
        annotation.proto.semantics.description = ""
      elif test_no == 3:
        test_description = "Missing semantics::trigger"
        annotation.proto.semantics.trigger = ""
      elif test_no == 4:
        test_description = "Missing semantics::data"
        annotation.proto.semantics.data = ""
      elif test_no == 5:
        test_description = "Missing semantics::destination"
        annotation.proto.semantics.destination = Destination.UNSPECIFIED
      elif test_no == 6:
        test_description = "Missing policy::cookies_allowed"
        annotation.proto.policy.cookies_allowed = CookiesAllowed.UNSPECIFIED
      elif test_no == 7:
        test_description = \
            "policy::cookies_allowed = NO with existing policy::cookies_store."
        annotation.proto.policy.cookies_allowed = CookiesAllowed.NO
        annotation.proto.policy.cookies_store = "somewhere"
      elif test_no == 8:
        test_description = \
            "policy::cookies_allowed = NO and no policy::cookies_store."
        annotation.proto.policy.cookies_allowed = CookiesAllowed.NO
        annotation.proto.policy.cookies_store = ""
        expect_error = False
      elif test_no == 9:
        test_description = \
            "policy::cookies_allowed = YES and policy::cookies_store exists."
        annotation.proto.policy.cookies_allowed = CookiesAllowed.YES
        annotation.proto.policy.cookies_store = "somewhere"
        expect_error = False
      elif test_no == 10:
        test_description = \
            "policy::cookies_allowed = YES and no policy::cookies_store."
        annotation.proto.policy.cookies_allowed = CookiesAllowed.YES
        annotation.proto.policy.cookies_store = ""
      elif test_no == 11:
        test_description = "Missing policy::setting."
        annotation.proto.policy.setting = ""
        expect_error = False
      elif test_no == 12:
        test_description = \
            "Missing chrome policy and " \
            "policy::policy_exception_justification."
        annotation.proto.policy.ClearField("chrome_policy")
        annotation.proto.policy.ClearField("chrome_device_policy")
        annotation.proto.policy.policy_exception_justification = ""
      elif test_no == 13:
        test_description = \
            "Missing chrome policy and existing " \
            "policy::policy_exception_justification."
        annotation.proto.policy.ClearField("chrome_policy")
        annotation.proto.policy.ClearField("chrome_device_policy")
        annotation.proto.policy.policy_exception_justification = "Because!"
        expect_error = False
      elif test_no == 14:
        test_description = \
            "Existing chrome policy and no " \
            "policy::policy_exception_justification."
        self.assertTrue(annotation.proto.policy.chrome_policy)
        annotation.proto.policy.policy_exception_justification = ""
        expect_error = False
      elif test_no == 15:
        test_description = \
            "Existing chrome policy and existing " \
            "policy::policy_exception_justification."
        self.assertTrue(annotation.proto.policy.chrome_policy)
        annotation.proto.policy.policy_exception_justification = "Because!"
      elif test_no == 16:
        test_description = "Missing semantics::internal::contacts"
        annotation.proto.semantics.internal.Clear()
      elif test_no == 17:
        test_description = "Missing semantics::internal::contacts::email"
        annotation.proto.semantics.internal.ClearField("contacts")
      elif test_no == 18:
        test_description = "Missing semantics::internal::user_data::type"
        annotation.proto.semantics.user_data.Clear()
      elif test_no == 19:
        test_description = "Empty value semantics::internal::user_data::type"
        annotation.proto.semantics.user_data.ClearField("type")
      elif test_no == 20:
        test_description = "Invalid format semantics::last_reviewed"
        annotation.proto.semantics.last_reviewed = "23-12-2023"
      elif test_no == 21:
        test_description = \
            "Existing chrome policy (device policy only) and " \
            "missing policy::policy_exception_justification."
        self.assertTrue(annotation.proto.policy.chrome_device_policy)
        annotation.proto.policy.ClearField("chrome_policy")
        annotation.proto.policy.policy_exception_justification = ""
        expect_error = False
      else:
        # Done checking individual test cases.
        break

      logger.debug("Testing: {}".format(test_description))

      self.auditor.extracted_annotations = [annotation]
      errors = self.auditor.check_annotation_contents()

      if expect_error:
        self.assertEqual(1, len(errors),
                         "test_no={}, errors={}".format(test_no, errors))
      else:
        self.assertEqual([], errors,
                         "test_no={}, errors={}".format(test_no, errors))

      annotations.append(annotation)

      if expect_error:
        expected_errors_count += 1

      test_no += 1

    # Check all.
    self.auditor.extracted_annotations = annotations
    errors = self.auditor.check_annotation_contents()
    self.assertEqual(expected_errors_count, len(errors))

  def test_is_completable_with(self):
    """Tests is Annotation.is_completable_with() works as expected."""
    T = Annotation.Type
    for type1, type2 in itertools.product(*[list(T)] * 2):
      for ids in range(256):
        annotation1 = self.create_annotation_sample(type1, ids % 4,
                                                    (ids >> 2) % 4)
        annotation2 = self.create_annotation_sample(type2, (ids >> 4) % 4,
                                                    (ids >> 6))
        expectation = False
        if annotation1.type == T.PARTIAL and annotation1.second_id:
          expectation = expectation or \
            (annotation2.type  == T.COMPLETING and
             annotation1.second_id_hash_code == annotation2.unique_id_hash_code)
          expectation = expectation or \
            (annotation2.type  == T.BRANCHED_COMPLETING and
             annotation1.second_id_hash_code == annotation2.second_id_hash_code)
        self.assertEqual(annotation1.is_completable_with(annotation2),
                         expectation, "{} <=> {}".format(type1, type2))

  def test_create_complete_annotation(self):
    """Tests is Annotation.create_complete_annotation() works as
    expected."""
    instance = self.create_annotation_sample()
    other = self.create_annotation_sample()

    instance.proto.semantics.Clear()
    instance.proto.policy.ClearField('chrome_policy')
    other.proto.policy.ClearField('chrome_device_policy')

    instance.type = Annotation.Type.PARTIAL
    other.type = Annotation.Type.COMPLETING

    # Partial and Completing.
    instance.second_id = "SomeID"
    other.unique_id = "SomeID"
    combination, errors = instance.create_complete_annotation(other)
    self.assertEqual([], errors)
    self.assertEqual(combination.unique_id_hash_code,
                     instance.unique_id_hash_code)

    # Partial and Branched-completing.
    other.type = Annotation.Type.BRANCHED_COMPLETING
    other.second_id = "SomeID"
    instance.second_id = "SomeID"
    self.assertEqual(len(instance.proto.policy.chrome_policy), 0)
    self.assertEqual(len(instance.proto.policy.chrome_device_policy), 1)
    self.assertEqual(len(other.proto.policy.chrome_policy), 1)
    self.assertEqual(len(other.proto.policy.chrome_device_policy), 0)
    combination, errors = instance.create_complete_annotation(other)
    self.assertEqual([], errors)
    self.assertEqual(combination.unique_id_hash_code, other.unique_id_hash_code)
    self.assertEqual(len(combination.proto.policy.chrome_policy), 1)
    self.assertEqual(len(combination.proto.policy.chrome_device_policy), 1)

    # Inconsistent field.
    Destination = traffic_annotation.TrafficSemantics.Destination
    other.proto.MergeFrom(instance.proto)
    other.type = Annotation.Type.BRANCHED_COMPLETING
    other.second_id = "SomeID"
    instance.second_id = "SomeID"
    instance.proto.semantics.destination = Destination.WEBSITE
    other.proto.semantics.destination = Destination.LOCAL
    annotation, errors = instance.create_complete_annotation(other)
    self.assertEqual(1, len(errors))

  def test_load_from_archive(self):
    """Tests that Annotation.load_from_archive() works as expected."""
    archived = ArchivedAnnotation(type=Annotation.Type.PARTIAL,
                                  id="foobar",
                                  second_id="baz",
                                  content_hash_code=32,
                                  os_list=["linux", "windows"],
                                  added_in_milestone=62,
                                  semantics_fields=[2, 3],
                                  policy_fields=[-1, 3, 4],
                                  file_path=Path("foobar.cc"))
    annotation = Annotation.load_from_archive(archived)
    self.assertTrue(annotation.is_loaded_from_archive)
    self.assertEqual(annotation.type, archived.type)
    self.assertEqual(annotation.unique_id, archived.id)
    self.assertEqual(annotation.unique_id_hash_code, archived.hash_code)
    self.assertEqual(annotation.second_id, archived.second_id)
    self.assertEqual(annotation.archived_content_hash_code, 32)
    self.assertEqual(annotation.archived_added_in_milestone, 62)
    self.assertEqual(annotation.get_semantics_field_numbers(),
                     archived.semantics_fields)
    self.assertEqual(annotation.get_policy_field_numbers(),
                     archived.policy_fields)
    self.assertEqual(annotation.file, archived.file_path)

  def test_annotations_xml(self):
    """Tests is annotations.xml has proper content."""
    # annotations.xml should parse without errors.
    exporter = Exporter(get_current_platform())
    exporter.load_annotations_xml()
    errors = exporter.check_archived_annotations()
    self.assertEqual([], errors)

    # The content of annotations.xml shouldn't change when writing it.
    old_xml = Exporter.ANNOTATIONS_XML_PATH.read_text(encoding="utf-8")
    new_xml = exporter._generate_serialized_xml()
    self.assertEqual(old_xml, new_xml)

  def test_grouping_xml(self):
    """Tests is grouping.xml has proper content."""
    # grouping.xml should parse without errors.
    exporter = Exporter(get_current_platform())
    exporter.load_grouping_xml(Exporter.GROUPING_XML_PATH)

    # The content of grouping.xml shouldn't change when writing it.
    old_xml = Exporter.GROUPING_XML_PATH.read_text(encoding="utf-8")
    new_xml = exporter._generate_serialized_grouping_xml()
    self.assertEqual(old_xml, new_xml)

  def test_grouping_required_fields_errors(self) -> None:
    """Tests is grouping.xml has no content."""
    # grouping.xml should parse with errors.
    grouping_erro_xml_path = \
      TEST_DATA_DIR / "test_required_field_error_grouping.xml"
    exporter = Exporter(get_current_platform())
    self.assertRaises(ValueError,
                      lambda: exporter.load_grouping_xml(grouping_erro_xml_path))

  def test_annotations_xml_differences(self):
    """Tests if annotations.xml changes are correctly reported."""
    exporter = Exporter(get_current_platform())

    xml1 = (TEST_DATA_DIR /
            "annotations_sample1.xml").read_text(encoding="utf-8")
    xml2 = (TEST_DATA_DIR /
            "annotations_sample2.xml").read_text(encoding="utf-8")
    xml3 = (TEST_DATA_DIR /
            "annotations_sample3.xml").read_text(encoding="utf-8")

    diff12 = exporter._get_xml_differences(xml1, xml2)
    diff13 = exporter._get_xml_differences(xml1, xml3)
    diff23 = exporter._get_xml_differences(xml2, xml3)

    expected_diff12 = (TEST_DATA_DIR /
                       "annotations_diff12.txt").read_text(encoding="utf-8")
    expected_diff13 = (TEST_DATA_DIR /
                       "annotations_diff13.txt").read_text(encoding="utf-8")
    expected_diff23 = (TEST_DATA_DIR /
                       "annotations_diff23.txt").read_text(encoding="utf-8")

    self.assertEqual(expected_diff12, diff12)
    self.assertEqual(expected_diff13, diff13)
    self.assertEqual(expected_diff23, diff23)

  def test_annotation_grouping(self):
    """Tests if an annotation is in test_grouping.xml or not."""
    grouping_xml_path = TEST_DATA_DIR / "test_grouping.xml"
    errors = self.auditor.run_all_checks([], True, grouping_xml_path)
    self.assertTrue(errors)
    grouping_xml_ids = self.auditor._get_grouping_xml_ids(grouping_xml_path)
    self.assertCountEqual([
        "foobar_policy_fetcher", "foobar_info_fetcher",
        "fizzbuzz_handle_front_end_messages", "fizzbuzz_hard_coded_data_source",
        "fizzbuzz_http_handler", "widget_grabber"
    ], grouping_xml_ids)

  def test_setup(self) -> None:
    """|self.sample_annotations| should include all those inside
    test_data/test_sample_annotations.cc"""
    expected = [
        "ok_annotation", "ok_annotation_only_owner", "syntax_error_annotation",
        "incomplete_error_annotation", "invalid_assignment_annotation",
        "partially_populated_safe_listed", "missing_all_new_field_safe_listed",
        "ok_new_fields_safe_listed", "missing_new_fields_not_safe_listed",
        "missing_email_not_safe_listed", "invalid_userdata_not_safe_listed"
    ]
    self.assertCountEqual(expected, self.sample_annotations.keys())

  def test_ensure_errors(self) -> None:
    """In the |test_sample_annotations.cc| there are some broken annotations.

    This test ensures that AuditorUI catches these errors by
    running from start to finish via |.main()|
    """
    # Suppress |self.auditor_ui.main()| prints to stdout.
    with open(os.devnull, "w") as devnull:
      sys.stdout = devnull
      self.assertEqual(1, self.auditor_ui.main())  # 1 indicates errors caught.
      sys.stdout = sys.__stdout__

  def test_result_ok(self) -> None:
    errors = self.auditor.parse_extractor_output(
        [self.sample_annotations["ok_annotation"]])

    # Assert that correct annotation has been extracted and is OK (no errors).
    self.assertTrue(self.auditor.extracted_annotations)
    self.assertFalse(errors)

  def test_syntax_error(self) -> None:
    errors = self.auditor.parse_extractor_output(
        [self.sample_annotations["syntax_error_annotation"]])

    self.assertTrue(errors)
    result = errors[0]
    self.assertEqual(ErrorType.SYNTAX, result.type)
    self.assertTrue("sender: \"Cloud Policy\"': Expected \"{\"" in str(result))

  def test_incomplete_error(self) -> None:
    self.auditor.parse_extractor_output(
        [self.sample_annotations["incomplete_error_annotation"]])

    self.assertTrue(self.auditor.extracted_annotations)
    errors = self.auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    result = errors[0]
    self.assertEqual(ErrorType.INCOMPLETE_ANNOTATION, result.type)

    expected_missing_fields = [
        "sender", "chrome_policy", "chrome_device_policy", "cookies_store",
        "policy_exception_justification"
    ]
    missing_fields = str(result).split("missing fields:",
                                       1)[1].lstrip().split(", ")
    self.assertCountEqual(expected_missing_fields, missing_fields)

  def test_invalid_date_format_errors(self) -> None:
    self.auditor.parse_extractor_output(
        [self.sample_annotations["invalid_assignment_annotation"]])

    self.assertTrue(self.auditor.extracted_annotations)
    errors = self.auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    result = errors[0]
    self.assertEqual(ErrorType.INVALID_DATE_FORMAT, result.type)

  def test_missing_new_fields_errors(self) -> None:
    """Annotation is Missing new fields, related class is not in safe_list.txt.
    Annotation check returns MISSING_NEW_FIELDS error."""
    self.auditor.parse_extractor_output(
        [self.sample_annotations["missing_new_fields_not_safe_listed"]])
    expected_error_msg = [
        'last_reviewed', 'internal::contacts', 'user_data::type'
    ]

    self.assertTrue(self.auditor.extracted_annotations)
    errors = self.auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    self.assertEqual(ErrorType.MISSING_NEW_FIELDS, errors[0].type)
    for text in expected_error_msg:
      self.assertTrue(errors[0].message.find(text) >= 0)

  def test_missing_email_error(self) -> None:
    """Annotation is Missing email value, related class is not in safe_list.txt.
    Annotation check returns MISSING_NEW_FIELDS error."""
    self.auditor.parse_extractor_output(
        [self.sample_annotations["missing_email_not_safe_listed"]])
    self.assertTrue(self.auditor.extracted_annotations)
    errors = self.auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    self.assertEqual(ErrorType.MISSING_NEW_FIELDS, errors[0].type)
    self.assertTrue(errors[0].message.find(
        'internal::contacts::email or internal::contacts::owners') >= 0)

  def test_user_data_unspecified(self) -> None:
    """Annotation user_data::type contains UNSPECIFIED value. Annotation Check
    returns INVALID_USER_DATA_TYPE error."""
    self.auditor.parse_extractor_output(
        [self.sample_annotations["invalid_userdata_not_safe_listed"]])
    self.assertTrue(self.auditor.extracted_annotations)
    errors = self.auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    self.assertEqual(ErrorType.INVALID_USER_DATA_TYPE, errors[0].type)

  def test_missing_new_fields_safe_listed_file(self) -> None:
    """Check annotation without new fields, related class is
    in safe_list.txt. Annotation Check does not return
    an new fields related error. """

    ## use real safe_list.txt
    auditor = Auditor(get_current_platform())
    auditor.parse_extractor_output(
        [self.sample_annotations["missing_all_new_field_safe_listed"]])
    self.assertTrue(auditor.extracted_annotations)
    errors = auditor.run_all_checks([], False, Exporter.GROUPING_XML_PATH)
    self.assertFalse(errors)

  def test_partially_populated_safe_listed_file(self) -> None:
    """Check annotation with last_reviewed but missing email fields,
    related class is in safe_list.txt. Check returns MISSING_NEW_FIELDS
    annotation error."""
    auditor = Auditor(get_current_platform())
    auditor.parse_extractor_output(
        [self.sample_annotations["partially_populated_safe_listed"]])
    self.assertTrue(auditor.extracted_annotations)
    errors = auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    for error in errors:
      self.assertEqual(ErrorType.MISSING_NEW_FIELDS, error.type)

  def test_ok_new_fields_safe_listed_file(self) -> None:
    """Annotation is complete with all new fields but still present in
    safe_list.txt. Check returns error to REMOVE_FROM_SAFE_LIST."""
    auditor = Auditor(get_current_platform())
    auditor.parse_extractor_output(
        [self.sample_annotations["ok_new_fields_safe_listed"]])
    self.assertTrue(auditor.extracted_annotations)
    errors = auditor.run_all_checks([], True, Exporter.GROUPING_XML_PATH)
    self.assertTrue(errors)
    self.assertEqual(ErrorType.REMOVE_FROM_SAFE_LIST, errors[0].type)

  def test_get_current_platform(self) -> None:
    host_platform = platform.system().lower()

    if host_platform == "windows":
      self.assertEqual("windows",
                       get_current_platform(TEST_DATA_DIR / "out" / "Debug"))
    elif host_platform == "linux":
      self.assertEqual("linux",
                       get_current_platform(TEST_DATA_DIR / "out" / "Debug"))
      self.assertEqual("android",
                       get_current_platform(TEST_DATA_DIR / "out" / "Android"))
    else:
      raise ValueError("Unrecognized host platform {}".format(host_platform))

  def test_write_annotations_tsv_file(self) -> None:
    annotation, errors = self.deserialize("good_complete_annotation.txt")
    self.assertEqual([], errors)
    annotations = [annotation]

    self.maxDiff = None
    tsv_path = Path(tempfile.mktemp())
    write_annotations_tsv_file(tsv_path, annotations, [])
    self.assertTrue(tsv_path.exists())
    tsv_contents = tsv_path.read_text(encoding="utf-8")
    expected_contents = """Unique ID\tLast Update\tSender\tDescription\tTrigger\tData\tDestination\tCookies Allowed\tCookies Store\tSetting\tChrome Policy\tComments\tSource File
supervised_user_refresh_token_fetcher\t\tSupervised Users\tFetches an OAuth2 refresh token scoped down to the Supervised User Sync scope and tied to the given Supervised User ID, identifying the Supervised User Profile to be created.\tCalled when creating a new Supervised User profile in Chromium to fetch OAuth credentials for using Sync with the new profile.\t"The request is authenticated with an OAuth2 access token identifying the Google account and contains the following information:
* The Supervised User ID, a randomly generated 64-bit identifier for the profile.
* The device name, to identify the refresh token in account management."\tGoogle\tNo\t\tUsers can disable this feature by toggling 'Let anyone add a person to Chrome' in Chromium settings, under People.\tSupervisedUserCreationEnabled: false, external_policy: ""\t\thttps://cs.chromium.org/chromium/src/?l=0
"""
    self.assertEqual(expected_contents, tsv_contents)

  def test_result_ok_only_owner(self) -> None:
    """Annotation is complete with all new fields, and uses an owners file
    instead of email for contact info. Check returns no errors related to
    contact email or other new fields."""
    self.auditor.parse_extractor_output(
        [self.sample_annotations["ok_annotation_only_owner"]])
    errors = self.auditor.run_all_checks([], False, Exporter.GROUPING_XML_PATH)

    # Assert that correct annotation has been extracted and is OK (no errors).
    self.assertTrue(self.auditor.extracted_annotations)
    self.assertFalse(errors)


if __name__ == "__main__":
  unittest.main()
