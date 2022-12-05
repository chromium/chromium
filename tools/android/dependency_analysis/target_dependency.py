# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of the graph module for a build target dependency graph."""

import class_dependency
import graph
import group_json_consts
import java_group

# Usually a class is in exactly one target, but due to jar_excluded_patterns and
# android_library_factory some are in two or three. If there is a class that is
# in more than 3 build targets, it will be removed from the build graph. Some
# examples include:
# - org.chromium.base.natives.GEN_JNI (in 100+ targets)
# - org.chromium.android_webview.ProductConfig (in 15+ targets)
# - org.chromium.content.R (in 60+ targets)
_MAX_CONCURRENT_BUILD_TARGETS = 3


class JavaTarget(java_group.JavaGroup):
    """A representation of a Java target."""


class JavaTargetDependencyGraph(graph.Graph[JavaTarget]):
    """A graph representation of the dependencies between Java build targets.

    A directed edge A -> B indicates that A depends on B.
    """

    def __init__(self, class_graph: class_dependency.JavaClassDependencyGraph):
        """Initializes a new target-level dependency graph
        by "collapsing" a class-level dependency graph into its targets.

        Args:
            class_graph: A class-level graph to collapse to a target-level one.
        """
        super().__init__()

        # Create list of all targets using class nodes
        # so we don't miss targets with no dependencies (edges).
        for class_node in class_graph.nodes:
            if len(class_node.build_targets) > _MAX_CONCURRENT_BUILD_TARGETS:
                continue
            for build_target in class_node.build_targets:
                self.add_node_if_new(build_target)

        for begin_class, end_class in class_graph.edges:
            if len(begin_class.build_targets) > _MAX_CONCURRENT_BUILD_TARGETS:
                continue
            if len(end_class.build_targets) > _MAX_CONCURRENT_BUILD_TARGETS:
                continue
            for begin_target in begin_class.build_targets:
                for end_target in end_class.build_targets:
                    # Avoid intra-target deps.
                    if begin_target == end_target:
                        continue

                    self.add_edge_if_new(begin_target, end_target)

                    begin_target_node = self.get_node_by_key(begin_target)
                    end_target_node = self.get_node_by_key(end_target)
                    assert begin_target_node is not None
                    assert end_target_node is not None
                    begin_target_node.add_class(begin_class)
                    end_target_node.add_class(end_class)
                    begin_target_node.add_class_dependency_edge(
                        end_target_node, begin_class, end_class)

    def create_node_from_key(self, key: str):
        """Create a JavaTarget node from the given key (target name)."""
        return JavaTarget(key)

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
