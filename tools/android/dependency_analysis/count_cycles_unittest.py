#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.count_cycles."""

import itertools
import unittest

import count_cycles
import graph


class TestFindCycles(unittest.TestCase):
    """Unit tests for find_cycles."""
    KEY_0 = '0'
    KEY_1 = '1'
    KEY_2 = '2'
    KEY_3 = '3'
    MAX_CYCLE_LENGTH = 10

    def test_no_self_cycles(self):
        """Tests that self-cycles are not considered.

        0 <---+
        ^     |
        |     v
        +---> 1 (plus, 0 and 1 have self-cycles)
        (one cycle, 010)
        """
        test_graph = graph.Graph()
        test_graph.add_edge_if_new(self.KEY_0, self.KEY_1)
        test_graph.add_edge_if_new(self.KEY_1, self.KEY_0)
        test_graph.add_edge_if_new(self.KEY_0, self.KEY_0)
        test_graph.add_edge_if_new(self.KEY_1, self.KEY_1)

        res = count_cycles.find_cycles(test_graph, self.MAX_CYCLE_LENGTH)
        expected_cycles = {
            2: 1,
        }
        for cycle_length, cycles in enumerate(res):
            self.assertEqual(len(cycles), expected_cycles.get(cycle_length, 0))

    def test_big_cycle(self):
        """Tests using a graph with one big cycle.

        0 -> 1
        ^    |
        |    v
        3 <- 2
        (one cycle, 01230)
        """
        test_graph = graph.Graph()
        test_graph.add_edge_if_new(self.KEY_0, self.KEY_1)
        test_graph.add_edge_if_new(self.KEY_1, self.KEY_2)
        test_graph.add_edge_if_new(self.KEY_2, self.KEY_3)
        test_graph.add_edge_if_new(self.KEY_3, self.KEY_0)

        res = count_cycles.find_cycles(test_graph, self.MAX_CYCLE_LENGTH)
        expected_cycles = {
            4: 1,
        }
        for cycle_length, cycles in enumerate(res):
            self.assertEqual(len(cycles), expected_cycles.get(cycle_length, 0))

    def test_multiple_cycles(self):
        """Tests using a graph with multiple cycles.

        0 -> 1
        ^    ^
        |    v
        +--- 2 -> 3
        (two cycles, 0120 and 121)
        """
        test_graph = graph.Graph()
        test_graph.add_edge_if_new(self.KEY_0, self.KEY_1)
        test_graph.add_edge_if_new(self.KEY_1, self.KEY_2)
        test_graph.add_edge_if_new(self.KEY_2, self.KEY_0)
        test_graph.add_edge_if_new(self.KEY_2, self.KEY_1)
        test_graph.add_edge_if_new(self.KEY_2, self.KEY_3)

        res = count_cycles.find_cycles(test_graph, self.MAX_CYCLE_LENGTH)
        expected_cycles = {
            2: 1,
            3: 1,
        }
        for cycle_length, cycles in enumerate(res):
            self.assertEqual(len(cycles), expected_cycles.get(cycle_length, 0))

    def test_complete_graph(self):
        """Tests using a complete graph on 4 nodes.

        +------------+
        v            |
        0 <> 1 <--+  |
        ^    ^    |  |
        |    v    v  |
        +--> 2 <> 3 <+
        (20 cycles,
        010, 020, 030, 121, 131, 232,
        0120, 0130, 0210, 0230, 0310, 0320, 1231, 1321,
        01230, 01320, 02130, 02310, 03120, 03210)
        """
        test_graph = graph.Graph()
        for ka, kb in itertools.permutations(
            [self.KEY_0, self.KEY_1, self.KEY_2, self.KEY_3], 2):
            test_graph.add_edge_if_new(ka, kb)

        res = count_cycles.find_cycles(test_graph, self.MAX_CYCLE_LENGTH)
        expected_cycles = {2: 6, 3: 8, 4: 6}
        for cycle_length, cycles in enumerate(res):
            self.assertEqual(len(cycles), expected_cycles.get(cycle_length, 0))

    def test_complete_graph_restricted_length(self):
        """Tests using a complete graph on 4 nodes with maximum cycle length 2.

        +------------+
        v            |
        0 <> 1 <--+  |
        ^    ^    |  |
        |    v    v  |
        +--> 2 <> 3 <+
        (6 cycles, 010, 020, 030, 121, 131, 232)
        """
        test_graph = graph.Graph()
        for ka, kb in itertools.permutations(
            [self.KEY_0, self.KEY_1, self.KEY_2, self.KEY_3], 2):
            test_graph.add_edge_if_new(ka, kb)

        res = count_cycles.find_cycles(test_graph, 2)
        expected_cycles = {2: 6}
        for cycle_length, cycles in enumerate(res):
            self.assertEqual(len(cycles), expected_cycles.get(cycle_length, 0))
