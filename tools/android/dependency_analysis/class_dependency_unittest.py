#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.class_dependency."""

import unittest
import unittest.mock

import class_dependency


class TestHelperFunctions(unittest.TestCase):
    """Unit tests for module-level helper functions."""
    def test_java_class_params_to_key(self):
        """Tests that the helper concatenates, separated with a dot."""
        result = class_dependency.java_class_params_to_key('pkg.name', 'class')
        self.assertEqual(result, 'pkg.name.class')

    def test_split_nested_class_from_key(self):
        """Tests that the helper correctly splits out a nested class."""
        part1, part2 = class_dependency.split_nested_class_from_key(
            'pkg.name.class$nested')
        self.assertEqual(part1, 'pkg.name.class')
        self.assertEqual(part2, 'nested')

    def test_split_nested_class_from_key_no_nested(self):
        """Tests that the helper works when there is no nested class."""
        part1, part2 = class_dependency.split_nested_class_from_key(
            'pkg.name.class')
        self.assertEqual(part1, 'pkg.name.class')
        self.assertIsNone(part2)

    def test_split_nested_class_from_key_lambda(self):
        """Tests that the helper works for jdeps' formatting of lambdas."""
        part1, part2 = class_dependency.split_nested_class_from_key(
            'pkg.name.class$$Lambda$1')
        self.assertEqual(part1, 'pkg.name.class')
        self.assertEqual(part2, '$Lambda$1')

    def test_split_nested_class_from_key_numeric(self):
        """Tests that the helper works for jdeps' formatting of nested classes.

        Specifically, jdeps uses a numeric name for private nested classes.
        """
        part1, part2 = class_dependency.split_nested_class_from_key(
            'pkg.name.class$1')
        self.assertEqual(part1, 'pkg.name.class')
        self.assertEqual(part2, '1')


class TestJavaClass(unittest.TestCase):
    """Unit tests for dependency_analysis.class_dependency.JavaClass."""
    TEST_PKG = 'package'
    TEST_CLS = 'class'
    UNIQUE_KEY_1 = 'abc'
    UNIQUE_KEY_2 = 'def'

    def test_initialization(self):
        """Tests that JavaClass is initialized correctly."""
        test_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        self.assertEqual(test_node.name, f'{self.TEST_PKG}.{self.TEST_CLS}')
        self.assertEqual(test_node.package, self.TEST_PKG)
        self.assertEqual(test_node.class_name, self.TEST_CLS)

    def test_equality(self):
        """Tests that two JavaClasses with the same package+class are equal."""
        test_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        equal_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        self.assertEqual(test_node, equal_node)

    def test_add_nested_class(self):
        """Tests adding a single nested class to this class."""
        test_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        test_node.add_nested_class(self.UNIQUE_KEY_1)
        self.assertEqual(test_node.nested_classes, {self.UNIQUE_KEY_1})

    def test_add_nested_class_multiple(self):
        """Tests adding multiple nested classes to this class."""
        test_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        test_node.add_nested_class(self.UNIQUE_KEY_1)
        test_node.add_nested_class(self.UNIQUE_KEY_2)
        self.assertEqual(test_node.nested_classes,
                         {self.UNIQUE_KEY_1, self.UNIQUE_KEY_2})

    def test_add_nested_class_duplicate(self):
        """Tests that adding the same nested class twice will not dupe."""
        test_node = class_dependency.JavaClass(self.TEST_PKG, self.TEST_CLS)
        test_node.add_nested_class(self.UNIQUE_KEY_1)
        test_node.add_nested_class(self.UNIQUE_KEY_1)
        self.assertEqual(test_node.nested_classes, {self.UNIQUE_KEY_1})


class TestJavaClassDependencyGraph(unittest.TestCase):
    """Unit tests for JavaClassDependencyGraph.

    Full name: dependency_analysis.class_dependency.JavaClassDependencyGraph.
    """
    def setUp(self):
        """Sets up a new JavaClassDependencyGraph."""
        self.test_graph = class_dependency.JavaClassDependencyGraph()

    def test_create_node_from_key(self):
        """Tests that a jdeps name is correctly parsed into package + class."""
        created_node = self.test_graph.create_node_from_key(
            'package.class$nested')
        self.assertEqual(created_node.package, 'package')
        self.assertEqual(created_node.class_name, 'class')
        self.assertEqual(created_node.name, 'package.class')


if __name__ == '__main__':
    unittest.main()
