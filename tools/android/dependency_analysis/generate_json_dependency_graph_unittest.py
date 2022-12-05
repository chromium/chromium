#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.generate_json_dependency_graph."""

import pathlib
import unittest

import generate_json_dependency_graph

GN_DESC_OUTPUT = """
//path/to/dep1:java
//path/to/dep1:java__build_config_crbug_908819
//path/to/dep1:java__compile_java
//path/to/dep1:java__dex
//path/to/dep2:java
//path/to/dep2:java__build_config_crbug_908819
//path/to/dep2:java__compile_java
//path/to/dep2:java__dex
//path/to/root:java
//path/to/root:java__build_config_crbug_908819
//path/to/root:java__compile_java
//path/to/root:java__dex
"""


class TestHelperFunctions(unittest.TestCase):
    """Unit tests for module-level helper functions."""

    def test_class_is_interesting(self):
        """Tests that the helper identifies a valid Chromium class name."""
        self.assertTrue(
            generate_json_dependency_graph.class_is_interesting(
                'org.chromium.chrome.browser.Foo',
                prefixes=('org.chromium.', )))

    def test_class_is_interesting_longer(self):
        """Tests that the helper identifies a valid Chromium class name."""
        self.assertTrue(
            generate_json_dependency_graph.class_is_interesting(
                'org.chromium.chrome.browser.foo.Bar',
                prefixes=('org.chromium.', )))

    def test_class_is_interesting_negative(self):
        """Tests that the helper ignores a non-Chromium class name."""
        self.assertFalse(
            generate_json_dependency_graph.class_is_interesting(
                'org.notchromium.chrome.browser.Foo',
                prefixes=('org.chromium.', )))

    def test_class_is_interesting_not_interesting(self):
        """Tests that the helper ignores a builtin class name."""
        self.assertFalse(
            generate_json_dependency_graph.class_is_interesting(
                'java.lang.Object', prefixes=('org.chromium.', )))

    def test_class_is_interesting_everything_interesting(self):
        """Tests that the helper allows anything when no prefixes are passed."""
        self.assertTrue(
            generate_json_dependency_graph.class_is_interesting(
                'java.lang.Object', prefixes=tuple()))

    def test_parse_original_targets_and_jars_legacy(self):
        result = generate_json_dependency_graph.parse_original_targets_and_jars(
            GN_DESC_OUTPUT, pathlib.Path('out/Test'), 761559)
        self.assertEqual(len(result), 3)
        self.assertEqual(
            result, {
                '//path/to/dep1:java':
                pathlib.Path('out/Test/gen/path/to/dep1/java.javac.jar'),
                '//path/to/dep2:java':
                pathlib.Path('out/Test/gen/path/to/dep2/java.javac.jar'),
                '//path/to/root:java':
                pathlib.Path('out/Test/gen/path/to/root/java.javac.jar')
            })

    def test_parse_original_targets_and_jars_current(self):
        # After crrev.com/c/2161205, *.javac.jar are in obj/
        result = generate_json_dependency_graph.parse_original_targets_and_jars(
            GN_DESC_OUTPUT, pathlib.Path('out/Test'), 761560)
        self.assertEqual(len(result), 3)
        self.assertEqual(
            result, {
                '//path/to/dep1:java':
                pathlib.Path('out/Test/obj/path/to/dep1/java.javac.jar'),
                '//path/to/dep2:java':
                pathlib.Path('out/Test/obj/path/to/dep2/java.javac.jar'),
                '//path/to/root:java':
                pathlib.Path('out/Test/obj/path/to/root/java.javac.jar')
            })

    def test_parse_original_targets_and_jars_branch(self):
        # A branch without Commit-Cr-Position should be considered modern
        result = generate_json_dependency_graph.parse_original_targets_and_jars(
            GN_DESC_OUTPUT, pathlib.Path('out/Test'), 0)
        self.assertEqual(len(result), 3)
        self.assertEqual(
            result, {
                '//path/to/dep1:java':
                pathlib.Path('out/Test/obj/path/to/dep1/java.javac.jar'),
                '//path/to/dep2:java':
                pathlib.Path('out/Test/obj/path/to/dep2/java.javac.jar'),
                '//path/to/root:java':
                pathlib.Path('out/Test/obj/path/to/root/java.javac.jar')
            })


class TestJavaClassJdepsParser(unittest.TestCase):
    """Unit tests for
        dependency_analysis.class_dependency.JavaClassJdepsParser.
    """

    BUILD_TARGET = '//build/target:1'

    def setUp(self):
        """Sets up a new JavaClassJdepsParser."""
        self.parser = generate_json_dependency_graph.JavaClassJdepsParser()

    def test_parse_line(self):
        """Tests that new nodes + edges are added after a successful parse."""
        self.parser.parse_line(
            self.BUILD_TARGET,
            'org.chromium.a -> org.chromium.b org.chromium.c')
        self.assertEqual(self.parser.graph.num_nodes, 2)
        self.assertEqual(self.parser.graph.num_edges, 1)

    def test_parse_line_not_interesting(self):
        """Tests that a dependency on an uninteresting class adds a node only
            for the origin class."""
        self.parser.parse_line(self.BUILD_TARGET, 'org.chromium.a -> b c')
        self.assertEqual(self.parser.graph.num_nodes, 1)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_too_short(self):
        """Tests that nothing is changed if the line is too short."""
        self.parser.parse_line(self.BUILD_TARGET, 'org.chromium.a -> b')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_not_found(self):
        """Tests that nothing is changed if the line contains `not found`
            as the second class.
        """
        self.parser.parse_line(self.BUILD_TARGET,
                               'org.chromium.a -> not found')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_empty_string(self):
        """Tests that nothing is changed if the line is empty."""
        self.parser.parse_line(self.BUILD_TARGET, '')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)

    def test_parse_line_bad_input(self):
        """Tests that nothing is changed if the line is nonsensical"""
        self.parser.parse_line(self.BUILD_TARGET, 'bad_input')
        self.assertEqual(self.parser.graph.num_nodes, 0)
        self.assertEqual(self.parser.graph.num_edges, 0)


if __name__ == '__main__':
    unittest.main()
