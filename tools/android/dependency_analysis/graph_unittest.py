#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.graph."""

import unittest
import graph


class TestNode(unittest.TestCase):
    """Unit tests for dependency_analysis.graph.Node."""
    UNIQUE_KEY_1 = 'abc'
    UNIQUE_KEY_2 = 'def'

    def test_initialization(self):
        """Tests that the node was initialized correctly."""
        test_node = graph.Node(self.UNIQUE_KEY_1)
        self.assertEqual(test_node.name, self.UNIQUE_KEY_1)
        self.assertEqual(len(test_node.inbound), 0)
        self.assertEqual(len(test_node.outbound), 0)

    def test_equality(self):
        """Tests that two nodes with the same unique keys are equal."""
        test_node = graph.Node(self.UNIQUE_KEY_1)
        equal_node = graph.Node(self.UNIQUE_KEY_1)
        self.assertEqual(test_node, equal_node)

    def test_add_outbound(self):
        """Tests adding a single outbound edge from the node."""
        begin_node = graph.Node(self.UNIQUE_KEY_1)
        end_node = graph.Node(self.UNIQUE_KEY_2)
        begin_node.add_outbound(end_node)
        self.assertEqual(begin_node.outbound, {end_node})

    def test_add_outbound_duplicate(self):
        """Tests that adding the same outbound edge twice will not dupe."""
        begin_node = graph.Node(self.UNIQUE_KEY_1)
        end_node = graph.Node(self.UNIQUE_KEY_2)
        begin_node.add_outbound(end_node)
        begin_node.add_outbound(end_node)
        self.assertEqual(begin_node.outbound, {end_node})

    def test_add_outbound_self(self):
        """Tests adding an circular outbound edge to the node itself."""
        test_node = graph.Node(self.UNIQUE_KEY_1)
        test_node.add_outbound(test_node)
        self.assertEqual(test_node.outbound, {test_node})

    def test_add_inbound(self):
        """Tests adding a single inbound edge to the node."""
        begin_node = graph.Node(self.UNIQUE_KEY_1)
        end_node = graph.Node(self.UNIQUE_KEY_2)
        end_node.add_inbound(begin_node)
        self.assertEqual(end_node.inbound, {begin_node})

    def test_add_inbound_duplicate(self):
        """Tests that adding the same inbound edge twice will not dupe."""
        begin_node = graph.Node(self.UNIQUE_KEY_1)
        end_node = graph.Node(self.UNIQUE_KEY_2)
        end_node.add_inbound(begin_node)
        end_node.add_inbound(begin_node)
        self.assertEqual(end_node.inbound, {begin_node})

    def test_add_inbound_self(self):
        """Tests adding an circular inbound edge from the node itself."""
        test_node = graph.Node(self.UNIQUE_KEY_1)
        test_node.add_inbound(test_node)
        self.assertEqual(test_node.inbound, {test_node})


class TestGraph(unittest.TestCase):
    """Unit tests for dependency_analysis.graph.Graph."""
    UNIQUE_KEY_1 = 'abc'
    UNIQUE_KEY_2 = 'def'

    def setUp(self):
        """Sets up a new graph object."""
        self.test_graph = graph.Graph()

    def test_initialization(self):
        """Tests that the graph was initialized correctly."""
        self.assertEqual(self.test_graph.num_nodes, 0)
        self.assertEqual(self.test_graph.num_edges, 0)
        self.assertEqual(self.test_graph.nodes, [])
        self.assertEqual(self.test_graph.edges, [])

    def test_get_node_exists(self):
        """Tests getting a node that we know exists in the graph."""
        self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        self.assertIsNotNone(self.test_graph.get_node_by_key(
            self.UNIQUE_KEY_1))

    def test_get_node_does_not_exist(self):
        """Tests getting a node that we know does not exist in the graph."""
        self.assertIsNone(self.test_graph.get_node_by_key(self.UNIQUE_KEY_1))

    def test_add_nodes(self):
        """Tests adding two different nodes to the graph."""
        node1 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        node2 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_2)
        self.assertEqual(self.test_graph.num_nodes, 2)
        self.assertEqual(graph.sorted_nodes_by_name(self.test_graph.nodes),
                         graph.sorted_nodes_by_name([node1, node2]))

    def test_add_nodes_duplicate(self):
        """Tests adding the same node twice to the graph."""
        self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        node = self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        self.assertEqual(self.test_graph.num_nodes, 1)
        self.assertEqual(self.test_graph.nodes, [node])

    def test_add_edge(self):
        """Tests adding a new edge to the graph."""
        node1 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        node2 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_2)
        self.test_graph.add_edge_if_new(self.UNIQUE_KEY_1, self.UNIQUE_KEY_2)

        self.assertEqual(self.test_graph.num_edges, 1)
        self.assertEqual(node2.inbound, {node1})
        self.assertEqual(node1.outbound, {node2})
        self.assertEqual(self.test_graph.edges, [(node1, node2)])

    def test_add_edge_double_sided(self):
        """Tests adding a bidirectional edge to the graph."""
        node1 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        node2 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_2)
        self.test_graph.add_edge_if_new(self.UNIQUE_KEY_1, self.UNIQUE_KEY_2)
        self.test_graph.add_edge_if_new(self.UNIQUE_KEY_2, self.UNIQUE_KEY_1)

        self.assertEqual(self.test_graph.num_edges, 2)
        self.assertEqual(node1.inbound, {node2})
        self.assertEqual(node1.outbound, {node2})
        self.assertEqual(node2.inbound, {node1})
        self.assertEqual(node2.outbound, {node1})
        self.assertEqual(
            graph.sorted_edges_by_name(self.test_graph.edges),
            graph.sorted_edges_by_name([(node1, node2), (node2, node1)]))

    def test_add_edge_duplicate(self):
        """Tests adding a duplicate edge to the graph."""
        node1 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_1)
        node2 = self.test_graph.add_node_if_new(self.UNIQUE_KEY_2)
        edge_added_first = self.test_graph.add_edge_if_new(
            self.UNIQUE_KEY_1, self.UNIQUE_KEY_2)
        edge_added_second = self.test_graph.add_edge_if_new(
            self.UNIQUE_KEY_1, self.UNIQUE_KEY_2)

        self.assertEqual(self.test_graph.num_edges, 1)
        self.assertTrue(edge_added_first)
        self.assertFalse(edge_added_second)
        self.assertEqual(self.test_graph.edges, [(node1, node2)])

    def test_add_edge_nodes_do_not_exist(self):
        """Tests adding a new edge to a graph without the edge's nodes."""
        self.test_graph.add_edge_if_new(self.UNIQUE_KEY_1, self.UNIQUE_KEY_2)
        self.assertEqual(self.test_graph.num_edges, 1)
        self.assertIsNotNone(self.test_graph.get_node_by_key(
            self.UNIQUE_KEY_1))
        self.assertIsNotNone(self.test_graph.get_node_by_key(
            self.UNIQUE_KEY_2))


if __name__ == '__main__':
    unittest.main()
