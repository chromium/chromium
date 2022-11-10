#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.target_dependency."""

from typing import List, Optional
import unittest
import unittest.mock

import target_dependency


def create_mock_java_class(targets: Optional[List[str]] = None,
                           pkg='package',
                           cls='class'):
    mock_class = unittest.mock.Mock()
    mock_class.class_name = cls
    mock_class.package = pkg
    mock_class.name = f'{pkg}.{cls}'
    mock_class.build_targets = targets
    return mock_class


class TestJavaTargetDependencyGraph(unittest.TestCase):
    """Unit tests for JavaTargetDependencyGraph.

    Full name: dependency_analysis.class_dependency.JavaTargetDependencyGraph.
    """
    TEST_TARGET1 = 'target1'
    TEST_TARGET2 = 'target2'
    TEST_CLS = 'class'

    def test_initialization(self):
        """Tests that initialization collapses a class dependency graph."""
        # Create three class nodes (1, 2, 3) in two targets: [1, 2] and [3].

        mock_class_node_1 = create_mock_java_class(targets=[self.TEST_TARGET1])
        mock_class_node_2 = create_mock_java_class(targets=[self.TEST_TARGET1])
        mock_class_node_3 = create_mock_java_class(targets=[self.TEST_TARGET2])

        # Create dependencies (1 -> 3) and (3 -> 2).
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [
            mock_class_node_1, mock_class_node_2, mock_class_node_3
        ]
        mock_class_graph.edges = [(mock_class_node_1, mock_class_node_3),
                                  (mock_class_node_3, mock_class_node_2)]

        test_graph = target_dependency.JavaTargetDependencyGraph(
            mock_class_graph)

        # Expected output: two-node target graph with a bidirectional edge.
        self.assertEqual(test_graph.num_nodes, 2)
        self.assertEqual(test_graph.num_edges, 2)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_TARGET1))
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_TARGET2))
        # Ensure there is a bidirectional edge.
        (edge_1_start, edge_1_end), (edge_2_start,
                                     edge_2_end) = test_graph.edges
        self.assertEqual(edge_1_start, edge_2_end)
        self.assertEqual(edge_2_start, edge_1_end)

    def test_initialization_no_dependencies(self):
        """Tests that a target with no external dependencies is included."""
        # Create one class node (1) in one target: [1].
        mock_class_node = create_mock_java_class(targets=[self.TEST_TARGET1])

        # Do not create any dependencies.
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [mock_class_node]
        mock_class_graph.edges = []

        test_graph = target_dependency.JavaTargetDependencyGraph(
            mock_class_graph)

        # Expected output: one-node package graph with no edges.
        self.assertEqual(test_graph.num_nodes, 1)
        self.assertEqual(test_graph.num_edges, 0)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_TARGET1))

    def test_initialization_internal_dependencies(self):
        """Tests that a target with only internal dependencies has no edges.

        It is not useful to include intra-target dependencies in a build target
        dependency graph.
        """
        # Create two class nodes (1, 2) in one target: [1, 2].
        mock_class_node_1 = create_mock_java_class(targets=[self.TEST_TARGET1])
        mock_class_node_2 = create_mock_java_class(targets=[self.TEST_TARGET1])

        # Create a dependency (1 -> 2).
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [mock_class_node_1, mock_class_node_2]
        mock_class_graph.edges = [(mock_class_node_1, mock_class_node_2)]

        test_graph = target_dependency.JavaTargetDependencyGraph(
            mock_class_graph)

        # Expected output: one-node package graph with no edges.
        self.assertEqual(test_graph.num_nodes, 1)
        self.assertEqual(test_graph.num_edges, 0)
        self.assertIsNotNone(test_graph.get_node_by_key(self.TEST_TARGET1))

    def test_initialization_allows_multiple_targets_per_class(self):
        """Tests that initialization handles a class in multiple targets."""
        # Create three class nodes (1, 2, 3) in in two targets [1, 2], [1, 3].

        mock_class_node_1 = create_mock_java_class(
            targets=[self.TEST_TARGET1, self.TEST_TARGET2])
        mock_class_node_2 = create_mock_java_class(targets=[self.TEST_TARGET1])
        mock_class_node_3 = create_mock_java_class(targets=[self.TEST_TARGET2])

        # Create dependencies (1 -> 3) and (3 -> 2).
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = [
            mock_class_node_1, mock_class_node_2, mock_class_node_3
        ]
        mock_class_graph.edges = [(mock_class_node_1, mock_class_node_3),
                                  (mock_class_node_3, mock_class_node_2)]

        test_graph = target_dependency.JavaTargetDependencyGraph(
            mock_class_graph)

        # Expected output: two-node target graph with a bidirectional edge and
        #                  no self edge: target1 <=> target2
        self.assertEqual(test_graph.num_nodes, 2)
        self.assertEqual(test_graph.num_edges, 2)
        target1_node = test_graph.get_node_by_key(self.TEST_TARGET1)
        target2_node = test_graph.get_node_by_key(self.TEST_TARGET2)
        self.assertIsNotNone(target1_node)
        self.assertIsNotNone(target2_node)
        # Ensure there is a bidirectional edge.
        (edge_1_start, edge_1_end), (edge_2_start,
                                     edge_2_end) = test_graph.edges
        self.assertEqual(edge_1_start, edge_2_end)
        self.assertEqual(edge_2_start, edge_1_end)

    def test_create_node_from_key(self):
        """Tests that a JavaTarget is correctly generated."""
        mock_class_graph = unittest.mock.Mock()
        mock_class_graph.nodes = []
        mock_class_graph.edges = []
        test_graph = target_dependency.JavaTargetDependencyGraph(
            mock_class_graph)

        created_node = test_graph.create_node_from_key(self.TEST_TARGET1)
        self.assertEqual(created_node.name, self.TEST_TARGET1)


if __name__ == '__main__':
    unittest.main()
