#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.package_dependency."""

import unittest
import unittest.mock

import package_dependency


def create_mock_java_class(pkg='package', cls='class'):
    """Returns a Mock of JavaClass.

    The fields `class_name`, `package`, and `name` will be initialized.
    """
    mock_class = unittest.mock.Mock()
    mock_class.class_name = cls
    mock_class.package = pkg
    mock_class.name = f'{pkg}.{cls}'
    return mock_class


class TestJavaPackageDependencyGraph(unittest.TestCase):
    """Unit tests for JavaPackageDependencyGraph.

    Full name: dependency_analysis.class_dependency.JavaPackageDependencyGraph.
    """
    TEST_PKG_1 = 'package1'
    TEST_PKG_2 = 'package2'
    TEST_CLS = 'class'

    def test_initialization(self):
        """Tests that initialization collapses a class dependency graph."""
        # Create three class nodes (1, 2, 3) in two packages: [1, 2] and [3].

        mock_class_node_1 = create_mock_java_class(pkg=self.TEST_PKG_1)
        mock_class_node_2 = create_mock_java_class(pkg=self.TEST_PKG_1)
        mock_class_node_3 = create_mock_java_class(pkg=self.TEST_PKG_2)

        # Create dependencies (1 -> 3) and (3 -> 2).
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [
            mock_class_node_1, mock_class_node_2, mock_class_node_3
        ]
        mock_class_graph.edges = [(mock_class_node_1, mock_class_node_3),
                                  (mock_class_node_3, mock_class_node_2)]

        test_graph = package_dependency.JavaPackageDependencyGraph(
            mock_class_graph)

        # Expected output: two-node package graph with a bidirectional edge.
        self.assertEqual(test_graph.num_nodes, 2)
        self.assertEqual(test_graph.num_edges, 2)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_PKG_1))
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_PKG_2))
        # Ensure there is a bidirectional edge.
        (edge_1_start, edge_1_end), (edge_2_start,
                                     edge_2_end) = test_graph.edges
        self.assertEqual(edge_1_start, edge_2_end)
        self.assertEqual(edge_2_start, edge_1_end)

    def test_initialization_no_dependencies(self):
        """Tests that a package with no external dependencies is included."""
        # Create one class node (1) in one package: [1].
        mock_class_node = create_mock_java_class(pkg=self.TEST_PKG_1)

        # Do not create any dependencies.
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [mock_class_node]
        mock_class_graph.edges = []

        test_graph = package_dependency.JavaPackageDependencyGraph(
            mock_class_graph)

        # Expected output: one-node package graph with no edges.
        self.assertEqual(test_graph.num_nodes, 1)
        self.assertEqual(test_graph.num_edges, 0)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_PKG_1))

    def test_initialization_internal_dependencies(self):
        """Tests that a package with only internal dependencies is included."""
        # Create two class nodes (1, 2) in one package: [1, 2].
        mock_class_node_1 = create_mock_java_class(pkg=self.TEST_PKG_1)
        mock_class_node_2 = create_mock_java_class(pkg=self.TEST_PKG_1)

        # Create a dependency (1 -> 2).
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [mock_class_node_1, mock_class_node_2]
        mock_class_graph.edges = [(mock_class_node_1, mock_class_node_2)]

        test_graph = package_dependency.JavaPackageDependencyGraph(
            mock_class_graph)

        # Expected output: one-node package graph with a self-edge.
        self.assertEqual(test_graph.num_nodes, 1)
        self.assertEqual(test_graph.num_edges, 1)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_PKG_1))
        # Ensure there is a self-edge.
        [(edge_start, edge_end)] = test_graph.edges
        self.assertEqual(edge_start, edge_end)

    def test_create_node_from_key(self):
        """Tests that a JavaPackage is correctly generated."""
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = []
        mock_class_graph.edges = []
        test_graph = package_dependency.JavaPackageDependencyGraph(
            mock_class_graph)

        created_node = test_graph.create_node_from_key(self.TEST_PKG_1)
        self.assertEqual(created_node.name, self.TEST_PKG_1)


if __name__ == '__main__':
    unittest.main()
