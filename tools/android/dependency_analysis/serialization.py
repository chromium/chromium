# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module for the de/serialization of graphs to/from files."""

import json
import pathlib
import sys
from typing import Dict, Tuple, Union

import class_dependency
import class_json_consts
import graph
import json_consts
import package_dependency
import target_dependency

_TOOLS_ANDROID_PATH = pathlib.Path(__file__).resolve().parents[1]
if str(_TOOLS_ANDROID_PATH) not in sys.path:
    sys.path.append(str(_TOOLS_ANDROID_PATH))
from python_utils import git_metadata_utils


def create_json_obj_from_node(node: graph.Node) -> Dict:
    """Generates a JSON representation of a given node.

    Structure:
    {
        'name': str,
        'meta': { see Node.get_node_metadata },
    }
    """
    json_obj: Dict[str, Union[str, dict]] = {
        json_consts.NAME: node.name,
    }
    node_meta = node.get_node_metadata()
    if node_meta is not None:
        json_obj[json_consts.META] = node_meta
    return json_obj


def create_json_obj_from_graph(graph_obj: graph.Graph) -> Dict:
    """Generates a JSON representation of the current graph.

    The list of nodes and edges is sorted in order to help with testing.
    Structure:
    {
        'nodes': [
            { see create_json_obj_from_node }, ...
        ],
        'edges': [
            {
                'begin': str,
                'end': str,
                'meta': { see Graph.get_edge_metadata },
            }, ...
        ],
    }
    """
    sorted_nodes = graph.sorted_nodes_by_name(graph_obj.nodes)
    json_nodes = [create_json_obj_from_node(node) for node in sorted_nodes]

    json_edges = []
    for begin_node, end_node in graph.sorted_edges_by_name(graph_obj.edges):
        edge_json_obj = {
            json_consts.BEGIN: begin_node.name,
            json_consts.END: end_node.name,
        }
        edge_meta = graph_obj.get_edge_metadata(begin_node, end_node)
        if edge_meta is not None:
            edge_json_obj[json_consts.META] = edge_meta
        json_edges.append(edge_json_obj)

    return {
        json_consts.NODES: json_nodes,
        json_consts.EDGES: json_edges,
    }


def create_class_graph_from_json_obj(
        json_obj: Dict) -> class_dependency.JavaClassDependencyGraph:
    """Creates a JavaClassDependencyGraph from a JSON representation."""
    class_graph = class_dependency.JavaClassDependencyGraph()

    for node_json_obj in json_obj[json_consts.NODES]:
        name = node_json_obj[json_consts.NAME]
        nested = node_json_obj[json_consts.META][
            class_json_consts.NESTED_CLASSES]
        build_targets = node_json_obj[json_consts.META][
            class_json_consts.BUILD_TARGETS]
        added_node = class_graph.add_node_if_new(name)
        added_node.nested_classes = set(nested)
        added_node.build_targets = set(build_targets)

    for edge_json_obj in json_obj[json_consts.EDGES]:
        begin_key = edge_json_obj[json_consts.BEGIN]
        end_key = edge_json_obj[json_consts.END]
        class_graph.add_edge_if_new(begin_key, end_key)

    return class_graph


def create_build_metadata(src_path: pathlib.Path) -> Dict:
    """Creates metadata about the build the graph was extracted from."""
    return {
        json_consts.COMMIT_HASH:
        git_metadata_utils.get_head_commit_hash(src_path),
        json_consts.COMMIT_CR_POSITION:
        git_metadata_utils.get_head_commit_cr_position(src_path),
        json_consts.COMMIT_TIME:
        git_metadata_utils.get_head_commit_time(src_path),
    }


def dump_class_and_package_and_target_graphs_to_file(
        class_graph: class_dependency.JavaClassDependencyGraph,
        package_graph: package_dependency.JavaPackageDependencyGraph,
        target_graph: target_dependency.JavaTargetDependencyGraph,
        filename: str, src_path: pathlib.Path):
    """Dumps a JSON representation of the class/package/target graph to a file.

    We dump the graphs together because the package graph in-memory holds
    references to class nodes (for storing class edges comprising
    a package edge), and hence the class graph is needed to recreate the
    package graph. Since our use cases always want the package graph over the
    class graph, there currently no point in dumping the class graph separately.

    Structure:
    {
        'class_graph': { see JavaClassDependencyGraph.to_json },
        'package_graph': { see JavaPackageDependencyGraph.to_json },
        'target_graph': { see JavaTargetDependencyGraph.to_json },
    }
    """
    json_obj = {
        json_consts.CLASS_GRAPH: create_json_obj_from_graph(class_graph),
        json_consts.PACKAGE_GRAPH: create_json_obj_from_graph(package_graph),
        json_consts.TARGET_GRAPH: create_json_obj_from_graph(target_graph),
        json_consts.BUILD_METADATA: create_build_metadata(src_path),
    }
    with open(filename, 'w') as json_file:
        json.dump(json_obj, json_file, separators=(',', ':'))


def load_class_graph_from_file(
        filename: str
) -> Tuple[class_dependency.JavaClassDependencyGraph, Dict]:
    """Recreates a JavaClassDependencyGraph from a JSON file.

    The file is expected to be in the format dumped by
    `dump_package_graph_to_file`.
    """
    with open(filename, 'r') as json_file:
        json_obj = json.load(json_file)
        class_graph_json_obj = json_obj[json_consts.CLASS_GRAPH]
        return create_class_graph_from_json_obj(
            class_graph_json_obj), json_obj.get(json_consts.BUILD_METADATA)


def load_class_and_package_graphs_from_file(
        filename: str
) -> Tuple[class_dependency.JavaClassDependencyGraph, package_dependency.
           JavaPackageDependencyGraph, Dict]:
    """Recreates a Java(Class+Package)DependencyGraph from a JSON file.

    The file is expected to be in the format dumped by
    `dump_class_and_package_graphs_to_file`.

    Note that we construct the package graph from the deserialized class graph,
    not using the serialized package graph at all. This aligns with how we
    construct the package graph when using jdeps. However, we still output
    a serialized package graph for other consumers of the JSON (eg. JS-side)
    which may want to bypass the costly conversion from class to package graph.
    """
    class_graph, metadata = load_class_graph_from_file(filename)
    package_graph = package_dependency.JavaPackageDependencyGraph(class_graph)
    return class_graph, package_graph, metadata
