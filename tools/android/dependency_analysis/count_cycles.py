#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Command-line tool to enumerate cycles in Graph structures."""

import argparse
import collections

from typing import Dict, List, Tuple

import serialization
import graph


Cycle = Tuple[graph.Node, ...]


def find_cycles_from_node(
    start_node: graph.Node,
    max_cycle_length: int,
    node_to_id: Dict[graph.Node, int],
) -> List[List[List[graph.Node]]]:
    """Finds all cycles starting at |start_node| in a subset of nodes.

    Only nodes with ID >= |start_node|'s ID will be considered. This ensures
    uniquely counting all cycles since this function is called on all nodes of
    the graph, one at a time in increasing order. Some justification: Consider
    cycle C with smallest node n. When this function is called on node n, C will
    be found since all nodes of C are >= n. After that call, C will never be
    found again since further calls are on nodes > n (n is removed from the
    search space).

    Cycles are found by recursively scanning all outbound nodes starting from
    |start_node|, up to a certain depth. Note this is the same idea, but is
    different from DFS since nodes can be visited more than once (to avoid
    missing cycles). An example of normal DFS (where nodes can only be visited
    once) missing cycles is in the following graph, starting at a:
    a <-> b <-> c
    ^           ^
    |           |
    +-----------+
    DFS(a)
        DFS(b)
            DFS(a) (cycle aba, return)
            DFS(c)
                DFS(b) (already seen, return)
                DFS(a) (cycle abca, return)
        DFS(c) (already seen, return)
    Since DFS(c) cannot proceed, we miss the cycles aca and acba.

    Args:
        start_node: The node to start the cycle search from. Only nodes with ID
          >= |start_node|'s ID will be considered.
        max_cycle_length: The maximum length of cycles to be found.
        node_to_id: A map from a Node to a generated ID.

    Returns:
        A list |cycles| of length |max_cycle_length| + 1, where cycles[i]
          contains all relevant cycles of length i.
    """
    start_node_id = node_to_id[start_node]
    cycles = [[] for _ in range(max_cycle_length + 1)]

    def edge_is_interesting(start: graph.Node, end: graph.Node) -> bool:
        if start == end:
            # Ignore self-loops.
            return False
        if node_to_id[end] < start_node_id:
            # Ignore edges ending at nodes with ID lower than the start.
            return False
        return True

    dfs_stack = collections.deque()
    on_stack: Dict[graph.Node, bool] = collections.defaultdict(bool)

    def find_cycles_dfs(cur_node: graph.Node, cur_length: int):
        for other_node in cur_node.outbound:
            if edge_is_interesting(cur_node, other_node):
                if other_node == start_node:
                    # We have found a valid cycle, add it to the list.
                    new_cycle = list(dfs_stack) + [cur_node, start_node]
                    cycles[cur_length + 1].append(new_cycle)

                elif (not on_stack[other_node]
                      and cur_length + 1 < max_cycle_length):
                    # We are only allowed to recurse into the next node if:
                    # 1) It hasn't been visited in the current cycle. This is
                    # because if the next node n _has_ been visited in the
                    # current cycle (i.e., it's on the stack), then we have
                    # found a cycle starting and ending at n. Since this
                    # function only returns cycles starting at |start_node|, we
                    # only care if |n = start_node| (which we already detect
                    # above).
                    # 2) It would not exceed the maximum depth allowed.
                    dfs_stack.append(cur_node)
                    on_stack[cur_node] = True
                    find_cycles_dfs(other_node, cur_length + 1)
                    dfs_stack.pop()
                    on_stack[cur_node] = False

    find_cycles_dfs(start_node, 0)
    return cycles


def find_cycles(base_graph: graph.Graph,
                max_cycle_length: int) -> List[List[Cycle]]:
    """Finds all cycles in the graph within a certain length.

    The algorithm is as such: Number the nodes arbitrarily. For i from 0 to
    the number of nodes, find all cycles starting and ending at node i using
    only nodes with numbers >= i (see find_cycles_from_node). Taking the union
    of the results will give all relevant cycles in the graph.

    Returns:
        A list |cycles| of length |max_cycle_length| + 1, where cycles[i]
          contains all cycles of length i.
    """
    sorted_base_graph_nodes = sorted(base_graph.nodes)
    # Some preliminary setup: map between the graph nodes' unique keys and a
    # unique number, since the algorithm needs some way to decide when a node is
    # 'bigger'. Nodes with a lower number will be processed first, which
    # influences the output cycles. For example, the cycle abca is also valid as
    # the cycle bcab or cabc. By numbering node a lower than b and c, it is
    # guaranteed that the cycle will be output as abca.
    node_to_id = {}
    for generated_node_id, node in enumerate(sorted_base_graph_nodes):
        node_to_id[node] = generated_node_id

    num_nodes = base_graph.num_nodes
    cycles = [[] for _ in range(max_cycle_length + 1)]

    for start_node in sorted_base_graph_nodes:
        start_node_cycles = find_cycles_from_node(start_node, max_cycle_length,
                                                  node_to_id)
        for cycle_length, cycle_list in enumerate(start_node_cycles):
            cycles[cycle_length].extend(cycle_list)

    # Convert cycles to be tuples of nodes, so the cycles are hashable and
    # immutable.
    immutable_cycles = []
    for cycle_list in cycles:
        immutable_cycles.append([tuple(cycle) for cycle in cycle_list])
    return immutable_cycles


def main():
    """Enumerates the cycles within a certain length in a graph."""

    arg_parser = argparse.ArgumentParser(
        description='Given a JSON dependency graph, count the number of cycles '
        'in the package graph.')
    required_arg_group = arg_parser.add_argument_group('required arguments')
    required_arg_group.add_argument(
        '-f',
        '--file',
        required=True,
        help='Path to the JSON file containing the dependency graph. '
        'See the README on how to generate this file.')
    required_arg_group.add_argument(
        '-l',
        '--cycle-length',
        type=int,
        required=True,
        help='The maximum length of cycles to find, at most 5 or 6 to keep the '
        'script runtime low.')
    arg_parser.add_argument(
        '-o',
        '--output',
        type=argparse.FileType('w'),
        help='Path to the file to write the list of cycles to.')
    args = arg_parser.parse_args()

    _, package_graph, _ = serialization.load_class_and_package_graphs_from_file(
        args.file)

    all_cycles = find_cycles(package_graph, args.cycle_length)
    # There are no cycles of length 0 or 1 (since self-loops are disallowed).
    nonzero_cycles = all_cycles[2:]

    print(f'Found {sum(len(cycles) for cycles in nonzero_cycles)} cycles.')

    for cycle_length, cycles in enumerate(nonzero_cycles, 2):
        print(f'Found {len(cycles)} cycles of length {cycle_length}.')

    if args.output is not None:
        print(f'Dumping cycles to {args.output.name}.')
        with args.output as output_file:
            for cycle_length, cycles in enumerate(nonzero_cycles, 2):
                output_file.write(f'Cycles of length {cycle_length}:\n')
                cycle_texts = []
                for cycle in cycles:
                    cycle_texts.append(' > '.join(cycle_node.name
                                                  for cycle_node in cycle))
                output_file.write('\n'.join(sorted(cycle_texts)))
                output_file.write('\n')


if __name__ == '__main__':
    main()
