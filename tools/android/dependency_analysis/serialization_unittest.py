#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for dependency_analysis.serialization."""

import unittest
import unittest.mock

import class_dependency
import class_json_consts
import graph
import json_consts
import package_dependency
import group_json_consts
import serialization


class TestSerialization(unittest.TestCase):
    """Unit tests for various de/serialization functions."""
    CLASS_1 = 'p1.c1'
    CLASS_2 = 'p1.c2'
    CLASS_3 = 'p2.c3'
    BUILD_TARGET_1 = '//build/target:one'
    BUILD_TARGET_2 = '//build/target:two'
    CLASS_1_NESTED_1 = 'abc'
    CLASS_1_NESTED_2 = 'def'
    CLASS_2_NESTED_1 = 'ghi'

    # The lists in the following JSON are sorted,
    # since we sort lists when serializing (for easier testing).
    JSON_CLASS_GRAPH = {
        json_consts.NODES: [
            {
                json_consts.NAME: CLASS_1,
                json_consts.META: {
                    class_json_consts.PACKAGE:
                    'p1',
                    class_json_consts.CLASS:
                    'c1',
                    class_json_consts.BUILD_TARGETS: [BUILD_TARGET_1],
                    class_json_consts.NESTED_CLASSES:
                    [CLASS_1_NESTED_1, CLASS_1_NESTED_2],
                },
            },
            {
                json_consts.NAME: CLASS_2,
                json_consts.META: {
                    class_json_consts.PACKAGE: 'p1',
                    class_json_consts.CLASS: 'c2',
                    class_json_consts.BUILD_TARGETS: [],
                    class_json_consts.NESTED_CLASSES: [CLASS_2_NESTED_1],
                },
            },
            {
                json_consts.NAME: CLASS_3,
                json_consts.META: {
                    class_json_consts.PACKAGE:
                    'p2',
                    class_json_consts.CLASS:
                    'c3',
                    class_json_consts.BUILD_TARGETS:
                    [BUILD_TARGET_1, BUILD_TARGET_2],
                    class_json_consts.NESTED_CLASSES: [],
                },
            },
        ],
        json_consts.EDGES: [
            {
                json_consts.BEGIN: CLASS_1,
                json_consts.END: CLASS_2,
            },
            {
                json_consts.BEGIN: CLASS_1,
                json_consts.END: CLASS_3,
            },
            {
                json_consts.BEGIN: CLASS_2,
                json_consts.END: CLASS_3,
            },
        ],
    }

    JSON_PACKAGE_GRAPH = {
        json_consts.NODES: [
            {
                json_consts.NAME: 'p1',
                json_consts.META: {
                    group_json_consts.CLASSES: [CLASS_1, CLASS_2],
                },
            },
            {
                json_consts.NAME: 'p2',
                json_consts.META: {
                    group_json_consts.CLASSES: [CLASS_3],
                },
            },
        ],
        json_consts.EDGES: [
            {
                json_consts.BEGIN: 'p1',
                json_consts.END: 'p1',
                json_consts.META: {
                    group_json_consts.CLASS_EDGES: [
                        [CLASS_1, CLASS_2],
                    ],
                },
            },
            {
                json_consts.BEGIN: 'p1',
                json_consts.END: 'p2',
                json_consts.META: {
                    group_json_consts.CLASS_EDGES: [
                        [CLASS_1, CLASS_3],
                        [CLASS_2, CLASS_3],
                    ],
                },
            },
        ],
    }

    def test_class_serialization(self):
        """Tests JSON serialization of a class dependency graph."""
        test_graph = class_dependency.JavaClassDependencyGraph()
        test_graph.add_edge_if_new(self.CLASS_1, self.CLASS_2)
        test_graph.add_edge_if_new(self.CLASS_1, self.CLASS_3)
        test_graph.add_edge_if_new(self.CLASS_2, self.CLASS_3)
        test_graph.get_node_by_key(self.CLASS_1).add_nested_class(
            self.CLASS_1_NESTED_1)
        test_graph.get_node_by_key(self.CLASS_1).add_nested_class(
            self.CLASS_1_NESTED_2)
        test_graph.get_node_by_key(self.CLASS_2).add_nested_class(
            self.CLASS_2_NESTED_1)
        test_graph.get_node_by_key(self.CLASS_1).add_build_target(
            self.BUILD_TARGET_1)
        test_graph.get_node_by_key(self.CLASS_3).add_build_target(
            self.BUILD_TARGET_1)
        test_graph.get_node_by_key(self.CLASS_3).add_build_target(
            self.BUILD_TARGET_2)

        test_json_obj = serialization.create_json_obj_from_graph(test_graph)

        self.assertEqual(test_json_obj, self.JSON_CLASS_GRAPH)

    def test_package_serialization(self):
        """Tests JSON serialization of a package dependency graph."""
        class_graph = class_dependency.JavaClassDependencyGraph()
        class_graph.add_edge_if_new(self.CLASS_1, self.CLASS_2)
        class_graph.add_edge_if_new(self.CLASS_1, self.CLASS_3)
        class_graph.add_edge_if_new(self.CLASS_2, self.CLASS_3)
        class_graph.get_node_by_key(self.CLASS_1).add_nested_class(
            self.CLASS_1_NESTED_1)
        class_graph.get_node_by_key(self.CLASS_1).add_nested_class(
            self.CLASS_1_NESTED_2)
        class_graph.get_node_by_key(self.CLASS_2).add_nested_class(
            self.CLASS_2_NESTED_1)

        package_graph = package_dependency.JavaPackageDependencyGraph(
            class_graph)
        test_json_obj = serialization.create_json_obj_from_graph(package_graph)

        self.assertEqual(test_json_obj, self.JSON_PACKAGE_GRAPH)

    def test_class_deserialization(self):
        """Tests JSON deserialization of a class dependency graph.

        Since we only ever construct package graphs from class graphs
        (and that feature is tested elsewhere), we do not need to test
        deserialization of package dependency graphs as well.
        """
        test_graph = serialization.create_class_graph_from_json_obj(
            self.JSON_CLASS_GRAPH)

        node_1 = test_graph.get_node_by_key(self.CLASS_1)
        node_2 = test_graph.get_node_by_key(self.CLASS_2)
        node_3 = test_graph.get_node_by_key(self.CLASS_3)

        self.assertIsNotNone(node_1)
        self.assertIsNotNone(node_2)
        self.assertIsNotNone(node_3)
        self.assertEqual(node_1.nested_classes,
                         {self.CLASS_1_NESTED_1, self.CLASS_1_NESTED_2})
        self.assertEqual(node_2.nested_classes, {self.CLASS_2_NESTED_1})
        self.assertEqual(node_3.nested_classes, set())
        self.assertEqual(node_1.build_targets, {self.BUILD_TARGET_1})
        self.assertEqual(node_2.build_targets, set())
        self.assertEqual(node_3.build_targets,
                         {self.BUILD_TARGET_1, self.BUILD_TARGET_2})
        self.assertEqual(
            graph.sorted_edges_by_name(test_graph.edges),
            graph.sorted_edges_by_name([(node_1, node_2), (node_1, node_3),
                                        (node_2, node_3)]))


if __name__ == '__main__':
    unittest.main()
