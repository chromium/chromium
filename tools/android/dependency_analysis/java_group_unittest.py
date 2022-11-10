#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.java_group."""

import unittest
import unittest.mock

import java_group


def create_mock_java_class(cls='class'):
    """Returns a Mock of JavaClass.

    The fields `class_name`, and `name` will be initialized.
    """
    mock_class = unittest.mock.Mock()
    mock_class.class_name = cls
    mock_class.name = f'package.{cls}'
    return mock_class


class TestJavaPackage(unittest.TestCase):
    """Unit tests for dependency_analysis.class_dependency.JavaGroup."""
    TEST_GRP_1 = 'group1'
    TEST_GRP_2 = 'group2'
    TEST_CLS_1 = 'class1'
    TEST_CLS_2 = 'class2'
    TEST_CLS_3 = 'class3'

    def test_initialization(self):
        """Tests that JavaGroup is initialized correctly."""
        test_node = java_group.JavaGroup(self.TEST_GRP_1)
        self.assertEqual(test_node.name, self.TEST_GRP_1)
        self.assertEqual(test_node.classes, {})

    def test_add_class(self):
        """Tests adding a single class to this group."""
        test_node = java_group.JavaGroup(self.TEST_GRP_1)
        mock_class_node = create_mock_java_class()
        test_node.add_class(mock_class_node)
        self.assertEqual(test_node.classes,
                         {mock_class_node.name: mock_class_node})

    def test_add_class_multiple(self):
        """Tests adding multiple classes to this group."""
        test_node = java_group.JavaGroup(self.TEST_GRP_1)
        mock_class_node_1 = create_mock_java_class(cls=self.TEST_CLS_1)
        mock_class_node_2 = create_mock_java_class(cls=self.TEST_CLS_2)
        test_node.add_class(mock_class_node_1)
        test_node.add_class(mock_class_node_2)
        self.assertEqual(
            test_node.classes, {
                mock_class_node_1.name: mock_class_node_1,
                mock_class_node_2.name: mock_class_node_2
            })

    def test_add_class_duplicate(self):
        """Tests that adding the same class twice will not dupe."""
        test_node = java_group.JavaGroup(self.TEST_GRP_1)
        mock_class_node = create_mock_java_class()
        test_node.add_class(mock_class_node)
        test_node.add_class(mock_class_node)
        self.assertEqual(test_node.classes,
                         {mock_class_node.name: mock_class_node})

    def test_get_class_dependencies_in_outbound_edge(self):
        """Tests adding/getting class dependency edges for a package edge."""
        start_node = java_group.JavaGroup(self.TEST_GRP_1)
        end_node = java_group.JavaGroup(self.TEST_GRP_2)

        # Create three class nodes (1, 2, 3)
        mock_class_node_1 = create_mock_java_class(cls=self.TEST_CLS_1)
        mock_class_node_2 = create_mock_java_class(cls=self.TEST_CLS_2)
        mock_class_node_3 = create_mock_java_class(cls=self.TEST_CLS_3)

        # Add the class dependencies (1 -> 2), (1 -> 2) (duplicate), (1 -> 3)
        start_node.add_class_dependency_edge(end_node, mock_class_node_1,
                                             mock_class_node_2)
        start_node.add_class_dependency_edge(end_node, mock_class_node_1,
                                             mock_class_node_2)
        start_node.add_class_dependency_edge(end_node, mock_class_node_1,
                                             mock_class_node_3)

        # Expected output: the two deduped dependencies (1 -> 2), (1 -> 3)
        # making up the edge (start_node, end_node).
        deps = start_node.get_class_dependencies_in_outbound_edge(end_node)
        self.assertEqual(len(deps), 2)
        self.assertEqual(
            deps, {(mock_class_node_1, mock_class_node_2),
                   (mock_class_node_1, mock_class_node_3)})

    def test_get_class_dependencies_in_outbound_edge_not_outbound(self):
        """Tests getting dependencies for a non-outbound edge."""
        test_node_1 = java_group.JavaGroup(self.TEST_GRP_1)
        test_node_2 = java_group.JavaGroup(self.TEST_GRP_2)

        # Expected output: an empty set, since there are no class dependencies
        # comprising a package dependency edge that doesn't exist.
        deps = test_node_1.get_class_dependencies_in_outbound_edge(test_node_2)
        self.assertEqual(deps, set())


if __name__ == '__main__':
    unittest.main()
