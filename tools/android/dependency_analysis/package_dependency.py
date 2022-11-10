# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of the graph module for a [Java package] dependency graph."""

import class_dependency
import graph
import group_json_consts
import java_group


class JavaPackage(java_group.JavaGroup):
    """A representation of a Java package."""


class JavaPackageDependencyGraph(graph.Graph[JavaPackage]):
    """A graph representation of the dependencies between Java packages.

    A directed edge A -> B indicates that A depends on B.
    """
    def __init__(self, class_graph: class_dependency.JavaClassDependencyGraph):
        """Initializes a new package-level dependency graph
        by "collapsing" a class-level dependency graph into its packages.

        Args:
            class_graph: A class-level graph to collapse to a package-level one.
        """
        super().__init__()

        # Create list of all packages using class nodes
        # so we don't miss packages with no dependencies (edges).
        for class_node in class_graph.nodes:
            self.add_node_if_new(class_node.package)

        for begin_class, end_class in class_graph.edges:
            begin_package = begin_class.package
            end_package = end_class.package
            self.add_edge_if_new(begin_package, end_package)

            begin_package_node = self.get_node_by_key(begin_package)
            end_package_node = self.get_node_by_key(end_package)
            assert begin_package_node is not None
            assert end_package_node is not None
            begin_package_node.add_class(begin_class)
            end_package_node.add_class(end_class)
            begin_package_node.add_class_dependency_edge(
                end_package_node, begin_class, end_class)

    def create_node_from_key(self, key: str):
        """Create a JavaPackage node from the given key (package name)."""
        return JavaPackage(key)

    def get_edge_metadata(self, begin_node, end_node):
        """Generates JSON metadata for the current edge.

        The list of edges is sorted in order to help with testing.
        Structure:
        {
            'class_edges': [
                [begin_key, end_key], ...
            ],
        }
        """
        return {
            group_json_consts.CLASS_EDGES:
            sorted(
                [begin.name, end.name] for begin, end in
                begin_node.get_class_dependencies_in_outbound_edge(end_node)),
        }
