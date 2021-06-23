#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Tests for the Traffic Annotation Auditor.
"""

import argparse
import itertools
import os
import sys
import unittest

from auditor import *

# Absolute path to chrome/src.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
TESTS_DIR = os.path.join(SCRIPT_DIR, "test_data")

# TODO(nicolaso): Move these to tools/traffic_annotation/scripts/test_data/ once
# the Python auditor has fully replaced the C++ one.
CPP_TESTS_DIR = os.path.join(SRC_DIR, "tools/traffic_annotation/auditor/tests")

TEST_DEPRECATED_IDS = [100, 101, 102]


class AuditorTest(unittest.TestCase):
  def setUp(self):
    unittest.TestCase.setUp(self)

    path_filters = [os.path.relpath(
      os.path.join(TESTS_DIR, "test_sample_annotations.cc"), SRC_DIR)]
    all_annotations = Auditor.run_extractor(path_filters)

    self.auditor_ui = AuditorUI(build_path, path_filters, False)

    self.auditor = Auditor()

    self.sample_annotations = {}
    for annotation in all_annotations:
      self.sample_annotations[annotation.unique_id] = annotation

  def deserialize(self, file_name: str) -> AnnotationInstance:
    file_path = os.path.join(CPP_TESTS_DIR, "extractor_outputs", file_name)
    with open(file_path) as f:
      lines = [l.rstrip() for l in f.readlines()]

    annotation = AnnotationInstance()
    extracted_annotation = extractor.Annotation(file_path=lines[0],
                                                line_number=int(lines[1]),
                                                type_name=lines[2],
                                                unique_id=lines[3],
                                                extra_id=lines[4],
                                                text="\n".join(lines[5:]))
    annotation.deserialize(extracted_annotation)
    return annotation

  def create_annotation_sample(self,
                               type=AnnotationInstance.Type.COMPLETE,
                               unique_id=0,
                               second_id=0) -> AnnotationInstance:
    if not unique_id:
      return self.deserialize("good_complete_annotation.txt")
    instance = AnnotationInstance()
    instance.type = type
    instance.proto.unique_id = "S{}".format(unique_id)
    instance.unique_id_hash_code = HashCode(unique_id)
    if second_id:
      instance.second_id = UniqueId("S{}".format(second_id))
      instance.second_id_hash_code = HashCode(second_id)
    else:
      instance.second_id = UniqueId("")
      instance.second_id_hash_code = HashCode(0)
    return instance

  def run_id_checker(self, annotation):
    id_checker = IdChecker(RESERVED_IDS, TEST_DEPRECATED_IDS)
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

  @unittest.skip("not yet implemented")
  def test_get_files_from_git(self):
    """Tests that FileFilter.get_files_from_git() returns correct files given
    a mock git_list.txt file. It also inherently checks
    FileFilter.is_file_relevant()."""
    filter = FileFilter()
    filter.git_file_for_testing = os.path.join(CPP_TESTS_DIR, "git_list.txt")
    filter.get_files_from_git(SRC_DIR)

    relevant_files = [
        "tools/traffic_annotation/auditor/tests/"
        "relevant_file_name_and_content.cc",
        "tools/traffic_annotation/auditor/tests/"
        "relevant_file_name_and_content.mm",
    ]
    self.assertCountEqual(relevant_files, filter.git_files)

  @unittest.skip("not yet implemented")
  def test_relevant_files_received(self):
    """Tests that FileFilter.get_relevant_files() gives the correct list of
    files, given a mock git_list.txt file."""
    filter = FileFilter()
    filter.git_file_for_testing = os.path.join(CPP_TESTS_DIR, "git_list.txt")
    filter.get_files_from_git(SRC_DIR)

    # Check if all files are returned with no ignore list and directory.
    ignore_list = []
    self.assertCountEqual(filter.git_files,
                          filter.get_relevant_files(ignore_list, ""))

    # Check if a file is ignored when added to the ignore list.
    ignore_list = filter.git_files[:1]
    self.assertCountEqual(
        set(filter.git_files) - ignore_list,
        filter.get_relevant_files(ignore_list, ""))

    # Check if files are filtered based on given directory.:w
    ignore_list = []
    self.assertCountEqual(
        filter.git_files,
        filter.get_relevant_files(ignore_list, "tools/traffic_annotation"))
    self.assertEqual([], filter.get_relevant_files(ignore_list, "content"))

  @unittest.skip("not yet implemented")
  def test_is_safelisted(self):
    """Tests if Auditor.is_safe_listed() works as expected. Inherently checks
    Auditor.load_safe_list() as well."""
    for t in Auditor.ExceptionType:
      # Anything in /tools directory is safelisted for all types.
      self.assertTrue(self.auditor.is_safe_listed("tools/something.cc", t))
      self.assertTrue(
          self.auditor.is_safe_listed("tools/somewhere/something.mm", t))

      # Anything in a general folder is not safelisted for any type.
      self.assertFalse(self.auditor.is_safe_listed("something.cc", t))
      self.assertFalse(self.auditor.is_safe_listed("content/something.mm", t))

    # Files defining missing annotation functions in net/ are exception of
    # 'missing' type.
    self.assertTrue(
        self.auditor.is_safe_listed("net/url_request/url_fetcher.cc",
                                    Auditor.ExceptionType.MISSING))
    self.assertTrue(
        self.auditor.is_safe_listed("net/url_request/url_request_context.cc",
                                    Auditor.ExceptionType.MISSING))

    # Files with the word "test" in their path can have the "test" annotation.
    self.assertFalse(
        self.auditor.is_safe_listed("net/url_request/url_fetcher.cc",
                                    Auditor.ExceptionType.TEST_ANNOTATION))
    self.assertTrue(
        self.auditor.is_safe_listed("chrome/browser/test_something.cc",
                                    Auditor.ExceptionType.TEST_ANNOTATION))
    self.assertTrue(
        self.auditor.is_safe_listed("test/send_something.cc",
                                    Auditor.ExceptionType.TEST_ANNOTATION))

  def test_annotation_deserialization(self) -> None:
    test_cases = [
        ("good_complete_annotation.txt", None,
         AnnotationInstance.Type.COMPLETE),
        ("good_branched_completing_annotation.txt", None,
         AnnotationInstance.Type.BRANCHED_COMPLETING),
        ("good_completing_annotation.txt", None,
         AnnotationInstance.Type.COMPLETING),
        ("good_partial_annotation.txt", None, AnnotationInstance.Type.PARTIAL),
        # TODO(nicolaso): Implement TEST_ANNOTATION and MISSING_TAG_USED.
        # ("good_test_annotation.txt", AuditorError.Type.TEST_ANNOTATION, None),
        # ("missing_annotation.txt", AuditorError.Type.MISSING_TAG_USED, None),
        ("fatal_annotation1.txt", "fatal", None),
        ("fatal_annotation2.txt", "fatal", None),
        ("fatal_annotation3.txt", "fatal", None),
        ("bad_syntax_annotation1.txt", AuditorError.Type.SYNTAX, None),
        ("bad_syntax_annotation2.txt", AuditorError.Type.SYNTAX, None),
        ("bad_syntax_annotation3.txt", AuditorError.Type.SYNTAX, None),
        ("bad_syntax_annotation4.txt", AuditorError.Type.SYNTAX, None),
    ]

    for test_case in test_cases:
      (file_name, error_type, annotation_type) = test_case

      if error_type == "fatal":
        # Should raise a fatal error (i.e., not an AuditorError).
        with self.assertRaises(Exception) as ecm:
          annotation = self.deserialize(file_name)
        self.assertNotIsInstance(ecm.exception, AuditorError)
      elif error_type:
        # Should raise an AuditorError of the specified type.
        with self.assertRaises(AuditorError) as cm:
          annotation = self.deserialize(file_name)
        self.assertEqual(error_type, cm.exception.type)
      else:
        # Should not raise an error.
        annotation = self.deserialize(file_name)
        self.assertEqual(annotation_type, annotation.type, file_name)

      # Check contents for one complete sample.
      if file_name != "good_complete_annotation.txt":
        continue

      self.assertEqual(annotation.proto.unique_id,
                       "supervised_user_refresh_token_fetcher")
      self.assertEqual(
          annotation.proto.source.file, "chrome/browser/supervised_user/legacy/"
          "supervised_user_refresh_token_fetcher.cc")
      self.assertEqual(annotation.proto.source.line, 166)
      self.assertEqual(annotation.proto.semantics.sender, "Supervised Users")
      self.assertEqual(annotation.proto.policy.cookies_allowed, 1)

  def test_get_reserved_ids_coverage(self):
    """Tests if RESERVED_IDS has all known ids."""
    self.assertEqual(["test", "test_partial", "missing"], RESERVED_IDS)

  @unittest.skip("not yet implemented")
  def test_reserved_ids_usage_detection(self):
    """Tests if use of reserved ids are detected."""
    for reserved_id in RESERVED_IDS:
      errors = self.run_id_checker(
          self.create_annotation_sample(AnnotationInstance.Type.COMPLETE,
                                        reserved_id))
      self.assertEqual(1, len(errors))
      self.assertEqual(AuditorError.Type.RESERVED_ID_HASH_CODE, errors[0].type)

      errors = self.run_id_checker(
          self.create_annotation_sample(AnnotationInstance.Type.PARTIAL, 1,
                                        reserved_id))
      self.assertEqual(1, len(errors))
      self.assertEqual(AuditorError.Type.RESERVED_ID_HASH_CODE, errors[0].type)

  @unittest.skip("not yet implemented")
  def test_deprecated_ids_usage_detection(self):
    """Tests if use of deprecated ids are detected."""
    for deprecated_id in TEST_DEPRECATED_IDS:
      errors = self.run_id_checker(
          self.create_annotation_sample(AnnotationInstance.Type.COMPLETE,
                                        deprecated_id))
      self.assertEqual(1, len(errors))
      self.assertEqual(AuditorError.Type.DEPRECATED_ID_HASH_CODE,
                       errors[0].type)

      errors = self.run_id_checker(
          self.create_annotation_sample(AnnotationInstance.Type.PARTIAL, 1,
                                        deprecated_id))
      self.assertEqual(1, len(errors))
      self.assertEqual(AuditorError.Type.DEPRECATED_ID_HASH_CODE,
                       errors[0].type)

  @unittest.skip("not yet implemented")
  def test_repeated_ids_detection(self):
    """Tests if use of repeated ids are detected."""
    id_checker = IdChecker([], [])

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
    self.assertContEqual([AuditorError.Type.REPEATED_ID] * 10,
                         [e.type for e in errors])

  @unittest.skip("not yet implemented")
  def test_similar_unique_and_second_ids_detection(self):
    """Tests if having the same unique id and second id is detected."""
    for t in AnnotationInstance.Type:
      errors = self.run_id_checker(self.create_annotation_sample(t, 1, 1))
      if annotation.needs_two_ids():
        self.assertEqual(1, len(errors), t)
      else:
        self.assertEqual(0, len(errors), t)

  @unittest.skip("not yet implemented")
  def test_duplicate_ids_detection(self):
    """Tests unique id and second id collision cases."""
    T = AnnotationInstance.Type
    for type1, type2 in itertools.product([list(T)] * 2):
      for id1, id2, id3, id4 in \
          itertools.product([range(1, 5) for i in range(4)]):
        logger.info("Testing ({}, {}, {}, {}, {}, {})", type1, type2, id1, id2,
                    id3, id4)
        annotation1 = self.create_annotation_sample(type1, id1, id2)
        annotation2 = self.create_annotation_sample(type2, id3, id4)
        id_checker = IdChecker([], [])
        errors = id_checker.check_ids([annotation1, annotation2])

        first_needs_two = annotation1.needs_two_ids()
        second_needs_two = annotation2.needs_two_ids()

        unique_ids = set([id1])
        if first_needs_two:
          unique_ids.add(id2)
        unique_ids.add(id3)
        if second_needs_two:
          unique_ids.add(id4)

        if first_needs_two and second_needs_two:
          # If both need 2 ids, either the 4 ids should be different, or the
          # second ids should be equal and both annotations should be of types
          # partial/branched-completing.
          if len(unique_ids) == 4:
            self.assertEqual([], errors)
          elif len(unique_ids) == 3:
            acceptable = (id2 == id4
                          and type1 in [T.PARTIAL, T.BRANCHED_COMPLETING]
                          and type2 in [T.PARTIAL, T.BRANCHED_COMPLETING])
            self.assertEqual(acceptable, bool(errors))
          else:
            self.assertEqual([], errors)
        elif first_needs_two and not second_needs_two:
          # If just the first one needs two ids, then either the 3 ids should be
          # different or the first annotation would be partial and teh second
          # completing, with one common id.
          if len(unique_ids) == 3:
            self.assertTrue(bool(errors))
          elif len(unique_ids) == 2:
            acceptable = (id2 == id3 and type1 in [T.PARTIAL, T.COMPLETING])
            self.assertEqual(acceptable, bool(errors))
          else:
            self.assertEqual([], errors)
        elif not first_needs_two and second_needs_two:
          # Can only be valid if all 3 are different.
          self.assertEqual(len(unique_id) == 3, bool(errors))
        else:
          # If none requires 2 ids, it can only be valid if ids are different.
          self.assertEqual(len(unique_ids) == 2, bool(errors))

  @unittest.skip("not yet implemented")
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
      annotation.type = AnnotationInstance.Type.COMPLETE
      annotation.proto.unique_id = test_case[0]
      annotation.unique_id_hash_code = 1
      errors = self.run_id_checker(annotation)
      self.assertEqual(test_case[1], bool(errors), test_case[0])

      # Set type to partial to require both ids.
      annotation.type = AnnotationInstance.Type.PARTIAL
      annotation.proto.unique_id = "Something_Good"
      annotation.second_id = test_case[0]
      annotation.unique_id_hash_code = 1
      annotation.second_id_hash_code = 2
      self.run_id_checker(annotation)
      self.assertEqual(test_case[1], bool(errors), test_case[0])

    # Test all cases together.
    annotations = []

    false_sample_count = 0
    for test_case in test_cases:
      annotation = self.create_annotation_sample()
      annotation.type = AnnotationInstance.Type.COMPLETE
      annotation.unique_id_hash_code = 1
      annotation.proto.unique_id = test_case[0]
      annotation.unique_id_hash_code += 1
      annotations.append(annotation)
      if not test_case[1]:
        false_sample_count += 1

    errors = self.run_id_checker(annotations)
    self.assertEqual(false_sample_count, errors)

  @unittest.skip("not yet implemented")
  def test_check_complete_annotations(self):
    """Tests if Auditor.check_annotations_contents() works as expected for
    COMPLETE annotations. It also inherently checks
    Auditor.is_annotation_complete(), Auditor.is_annotation_consistent(), and
    Auditor.is_in_grouping_xml()."""
    annotations = []
    expected_errors_count = 0

    CookiesAllowed = NetworkAnnotation.TrafficPolicy.CookiesAllowed

    test_no = 0
    while True:
      annotation = self.create_annotation_sample()
      annotation.proto.unique_id = "foobar_policy_fetcher"
      test_description = ""
      expect_error = True
      if test_no == 0:
        test_description = "All fields OK."
        expected_error = False
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
        annotation.proto.semantics.destination = ""
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
        break
      elif test_no == 12:
        test_description = \
            "Missing policy::chrome_policy and " \
            "policy::policy_exception_justification."
        annotation.proto.policy.chrome_policy = ""
        annotation.proto.policy.policy_exception_justification = ""
      elif test_no == 13:
        test_description = \
            "Missing policy::chrome_policy and existing " \
            "policy::policy_exception_justification."
        annotation.proto.policy.chrome_policy = ""
        annotation.proto.policy.policy_exception_justification = "Because!"
        expect_error = False
      elif test_no == 14:
        test_description = \
            "Existing policy::chrome_policy and no " \
            "policy::policy_exception_justification."
        self.assertTrue(annotation.proto.policy.chrome_policy)
        annotation.proto.policy.policy_exception_justification = ""
        expect_error = False
      elif test_no == 15:
        test_description = \
            "Existing policy::chrome_policy and existing" \
            "policy::policy_exception_justification."
        self.assertTrue(annotation.proto.policy.chrome_policy)
        annotation.proto.policy.policy_exception_justification = "Because!"
      else:
        # Done checking individual test cases.
        break

      logger.info("Testing: {}".format(test_description))

      self.auditor.errors = []
      self.auditor.extracted_annotations = [annotation]
      self.auditor.check_annotations_contents()

      if expected_error:
        self.assertEqual(1, len(self.auditor.errors))
      else:
        self.assertEqual([], self.auditor.errors)

      annotations.append(annotation)

      if expect_error:
        expected_errors_count += 1

    # Check all.
    self.auditor.errors = []
    self.auditor.extracted_annotations = annotations
    self.auditor.check_annotations_contents()
    self.assertEqual(expected_errors_count, len(self.auditor.errors))

  @unittest.skip("not yet implemented")
  def test_is_completable_with(self):
    """Tests is AnnotationInstance.is_completable_with() works as expected."""
    T = AnnotationInstance.Type
    for type1, type2 in itertools.product([list(T)] * 2):
      for ids in range(256):
        annotation1 = self.create_annotation_sample(type1, ids % 4,
                                                    (ids >> 2) % 4)
        annotation2 = self.create_annotation_sample(type2, (ids >> 4) % 4,
                                                    (ids >> 6))
        expectation = False
        if annotation1.type == T.PARTIAL and annotation1.second_id:
          expectation = expectation or \
            (annotation2.type  in [T.COMPLETING, T.BRANCHED_COMPLETING] and
             annotation1.second_id_hash_code == annotation2.unique_id_hash_code)
        self.assertEqual(annotation1.is_completable_with(annotation2),
                         expectation)

  @unittest.skip("not yet implemented")
  def test_create_complete_annotation(self):
    """Tests is AnnotationInstance.create_complete_annotation() works as
    expected."""
    instance = self.create_annotation_sample()
    other = self.create_annotation_sample()

    instance.proto.semantics.Clear()
    instance.proto.policy.Clear()

    instance.type = AnnotationInstance.Type.PARTIAL
    other.type = AnnotationInstance.Type.COMPLETING

    # Partial and Completing.
    instance.second_id_hash_code = 1
    instance.second_id = "SomeID"
    other.unique_id_hash_code = 1
    combination = instance.create_complete_annotation(other)
    self.assertEqual(combination.unique_id_hash_code,
                     instance.unique_id_hash_code)

    # Partial and Branched-completing.
    other.type = AnnotationInstance.Type.BRANCHED_COMPLETING
    instance.second_id_hash_code = 1
    other.second_id_hash_code = 1
    other.second_id = "SomeID"
    combination = instance.create_complete_annotation(other)
    self.assertEqual(combinatino.unique_id_hash_code, other.unique_id_hash_code)

    other.CopyFrom(instance)
    other.type = AnnotationInstance.Type.BRANCHED_COMPLETING
    instance.second_id_hash_code = 1
    other.second_id_hash_code = 1
    other.second_id = "SomeID"
    instance.proto.semantics.destination = \
        NetworkTrafficAnnotation.TrafficSemantics.Destination.WEBSITE
    other.proto.semantics.destination = \
        NetworkTrafficAnnotation.TrafficSemantics.Destination.LOCAL
    with self.assertRaises(AuditorError):
      instance.create_complete_annotation()

  @unittest.skip("not yet implemented")
  def test_annotations_xml(self):
    """Tests is annotations.xml has proper content."""
    exporter = Exporter(SRC_DIR)
    exporter.load_annotations_xml()
    errors = exporter.check_archived_annotations()
    self.assertEqual([], errors)

  @unittest.skip("not yet implemented")
  def test_annotations_xml_differences(self):
    """Tests if annotations.xml changes are correctly reported."""
    exporter = Exporter(SRC_DIR)

    with open(os.path.join(CPP_TESTS_DIR, "annotations_sample1.txt")) as f:
      xml1 = f.read()
    with open(os.path.join(CPP_TESTS_DIR, "annotations_sample2.txt")) as f:
      xml2 = f.read()
    with open(os.path.join(CPP_TESTS_DIR, "annotations_sample3.txt")) as f:
      xml3 = f.read()

    diff12 = exporter.get_xml_differences(xml1, xml2)
    diff13 = exporter.get_xml_differences(xml1, xml3)
    diff23 = exporter.get_xml_differences(xml2, xml3)

    with open(os.path.join(CPP_TESTS_DIR, "annotations_diff12.txt")) as f:
      expected_diff12 = f.read()
    with open(os.path.join(CPP_TESTS_DIR, "annotations_diff13.txt")) as f:
      expected_diff13 = f.read()
    with open(os.path.join(CPP_TESTS_DIR, "annotations_diff23.txt")) as f:
      expected_diff23 = f.read()

    self.assertEqual(expected_diff12, diff12)
    self.assertEqual(expected_diff13, diff13)
    self.assertEqual(expected_diff23, diff23)

  @unittest.skip("not yet implemented")
  def test_annotation_grouping(self):
    """Tests if an annotation is in test_grouping.xml or not."""
    grouping_xml_path = os.path.join(CPP_TESTS_DIR, "test_grouping.xml")
    annotation_unique_ids = self.auditor.get_grouping_annotations_unique_ids(
        grouping_xml_path)
    self.assertCountEqual([
        "foobar_policy_fetcher", "foobar_info_fetcher",
        "fizzbuzz_handle_front_end_messages", "fizzbuzz_hard_coded_data_source",
        "fizzbuzz_http_handler", "widget_grabber"
    ], annotation_unique_ids)

  def test_setup(self) -> None:
    """|self.sample_annotations| should include all those inside
    test_data/test_sample_annotations.cc"""
    expected = [
      "ok_annotation", "syntax_error_annotation", "incomplete_error_annotation"]
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
    self.auditor.parse_extractor_output(
      [self.sample_annotations["ok_annotation"]])

    # Assert that correct annotation has been extracted and is OK (no errors).
    self.assertTrue(self.auditor.extracted_annotations)
    self.assertFalse(self.auditor.errors)

  def test_syntax_error(self) -> None:
    self.auditor.parse_extractor_output(
      [self.sample_annotations["syntax_error_annotation"]])

    self.assertTrue(self.auditor.errors)
    result = self.auditor.errors[0]
    self.assertEqual(AuditorError.Type.SYNTAX, result.type)
    self.assertTrue(
      "sender: \"Cloud Policy\"': Expected \"{\"" in str(result))

  def test_incomplete_error(self) -> None:
    self.auditor.parse_extractor_output(
      [self.sample_annotations["incomplete_error_annotation"]])

    self.assertTrue(self.auditor.extracted_annotations)
    self.auditor.run_all_checks()
    self.assertTrue(self.auditor.errors)
    result = self.auditor.errors[0]
    self.assertEqual(AuditorError.Type.INCOMPLETE_ANNOTATION, result.type)

    expected_missing_fields = [
      "sender", "chrome_policy", "cookies_store",
      "policy_exception_justification"]
    missing_fields = str(result).split(
      "missing fields:", 1)[1].lstrip().split(", ")
    self.assertCountEqual(expected_missing_fields, missing_fields)


if __name__ == "__main__":
  args_parser = argparse.ArgumentParser(
    description="Unittests for auditor.py")
  args_parser.add_argument("--build-path",
                           help="Path to the build directory.",
                           required=True)
  args_parser.add_argument('unittest_args', nargs='*')

  args = args_parser.parse_args()
  build_path = args.build_path
  sys.argv[1:] = args.unittest_args
  unittest.main()
