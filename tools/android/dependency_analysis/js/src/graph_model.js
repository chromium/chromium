// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @file Data structures for representing a directed graph.
 */

/** Some aspects of the node's state, to help with node visualization. */
class NodeVisualizationState {
  constructor() {
    /** @public {boolean} */
    this.selectedByFilter = false;
    /** @public {boolean} */
    this.selectedByInbound = false;
    /** @public {boolean} */
    this.selectedByOutbound = false;
    /** @public {number} */
    this.inboundDepth = 0;
    /** @public {number} */
    this.outboundDepth = 0;
  }
}

/** A node in a directed graph. */
class GraphNode {
  /**
   * @param {string} id The unique ID for the node.
   * @param {string} displayName The name to display when visualizing this node.
   */
  constructor(id, displayName) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {string} */
    this.displayName = displayName;

    // Sets a mutable state to help with visualizing the node.
    this.visualizationState = new NodeVisualizationState();

    // This information (edges) already exists in Graph.edges. However, we
    // duplicate it here to make BFS traversal more efficient.
    /** @public {!Set<!GraphNode>} */
    this.inbound = new Set();
    /** @public {!Set<!GraphNode>} */
    this.outbound = new Set();
  }

  /**
   * Resets the mutable visualization state back to its default.
   */
  resetVisualizationState() {
    this.visualizationState = new NodeVisualizationState();
  }

  /**
   * Adds a node to the inbound set of this node.
   *
   * @param {!GraphNode} other The inbound node.
   */
  addInbound(other) {
    this.inbound.add(other);
  }

  /**
   * Adds a node to the outbound set of this node.
   *
   * @param {!GraphNode} other The outbound node.
   */
  addOutbound(other) {
    this.outbound.add(other);
  }
}

/** A node representing a Java class. */
class ClassNode extends GraphNode {
  constructor(id, displayName, packageName, buildTargets) {
    super(id, displayName);

    /** @public {string} */
    this.packageName = packageName;
    /** @public {!Array<string>} */
    this.buildTargets = buildTargets;
  }
}

/** A node representing a Java package. */
class PackageNode extends GraphNode {
  constructor(id, displayName, classNames) {
    super(id, displayName);

    /** @public {!Array<string>} */
    this.classNames = classNames;
  }
}

/** A node representing a Java build target. */
class TargetNode extends GraphNode {
  constructor(id, displayName, classNames) {
    super(id, displayName);

    /** @public {!Array<string>} */
    this.classNames = classNames;
  }
}

/**
 * An edge in a directed graph.
 */
class GraphEdge {
  /**
   * @param {string} id The unique ID for the edge.
   * @param {!GraphNode} source The source GraphNode object.
   * @param {!GraphNode} target The target GraphNode object.
   */
  constructor(id, source, target) {
    /** @public @const {string} */
    this.id = id;
    /** @public @const {!GraphNode} */
    this.source = source;
    /** @public @const {!GraphNode} */
    this.target = target;
  }
}

/**
 * The graph data for d3 to visualize.
 *
 * @typedef {object} D3GraphData
 * @property {!Array<!GraphNode>} nodes The nodes to visualize.
 * @property {!Array<!GraphEdge>} edges The edges to visualize.
 */
let D3GraphData;

/**
 * Generates and returns a unique edge ID from its source/target GraphNode IDs.
 *
 * This is used as an SVG element ID, so it must adhere to ID requirements
 * (unique, non-empty, no whitespace).
 *
 * @param {string} sourceId The ID of the source node.
 * @param {string} targetId The ID of the target node.
 * @return {string} The ID uniquely identifying the edge source -> target.
 */
function getEdgeIdFromNodes(sourceId, targetId) {
  return `${sourceId}>${targetId}`;
}

/** A directed graph. */
class GraphModel {
  constructor() {
    /** @public {!Map<string, !GraphNode>} */
    this.nodes = new Map();
    /** @public {!Map<string, !GraphEdge>} */
    this.edges = new Map();
  }

  /**
   * Adds a GraphNode to the node set.
   *
   * @param {!GraphNode} node The node to add.
   */
  addNodeIfNew(node) {
    if (!this.nodes.has(node.id)) {
      this.nodes.set(node.id, node);
    }
  }

  /**
   * Retrieves a GraphNode from the node set, if it exists.
   *
   * @param {string} id The ID of the desired node.
   * @return {?GraphNode} The GraphNode if it exists, otherwise null.
   */
  getNodeById(id) {
    return this.nodes.get(id) || null;
  }

  /**
   * Retrieves a GraphEdge from the edge set, if it exists.
   *
   * @param {string} id The ID of the desired edge.
   * @return {?GraphEdge} The GraphEdge if it exists, otherwise null.
   */
  getEdgeById(id) {
    return this.edges.get(id) || null;
  }

  /**
   * Creates and adds an GraphEdge to the edge set.
   * Also updates the inbound/outbound sets of the edge's nodes.
   *
   * @param {!GraphNode} sourceNode The node at the start of the edge.
   * @param {!GraphNode} targetNode The node at the end of the edge.
   */
  addEdgeIfNew(sourceNode, targetNode) {
    const edgeId = getEdgeIdFromNodes(sourceNode.id, targetNode.id);
    if (!this.edges.has(edgeId)) {
      const edge = new GraphEdge(edgeId, sourceNode, targetNode);
      this.edges.set(edgeId, edge);
      sourceNode.addOutbound(targetNode);
      targetNode.addInbound(sourceNode);
    }
  }

  /**
   * Generates the lists of nodes and edges for visualization with d3.
   *
   * The filter has three params: includedNodes (set of nodes), inbound (num),
   * and outbound (num). For nodes, we display `includedNodes` + the nodes
   * reachable within `inbound` inbound edges from `includedNodes` + the nodes
   * reachable within `outbound` outbound edges from `includedNodes`. For edges,
   * we display edges between all nodes except for the outermost layer, where we
   * only display the edges used to reach it from the second-outermost layer.
   *
   * @param {!Set<string>} includedNodeSet The nodes included in the filter.
   * @param {number} inboundDepth The maximum inbound distance.
   * @param {number} outboundDepth The maximum outbound distance.
   * @return {!D3GraphData} The nodes and edges to visualize.
   */
  getDataForD3(includedNodeSet, inboundDepth, outboundDepth) {
    // These will be updated throughout the function and returned at the end.
    const /** !Set<!GraphNode> */ resultNodeSet = new Set();
    const /** !Set<!GraphNode> */ resultEdgeSet = new Set();

    for (const node of this.nodes.values()) {
      node.resetVisualizationState();
    }

    // Initialize the inbound and outbound BFS by setting the "seen" collection
    // to the filter nodes. We maintain both a Set and array for efficiency.
    const /** !Set<string> */ inboundSeenNodes = new Set(includedNodeSet);
    const /** !Set<string> */ outboundSeenNodes = new Set(includedNodeSet);
    const /** !Array<!GraphNode> */ inboundNodeQueue = [];
    const /** !Array<!GraphNode> */ outboundNodeQueue = [];
    for (const nodeName of includedNodeSet) {
      const node = this.nodes.get(nodeName);
      if (node !== undefined) {
        node.visualizationState.selectedByFilter = true;
        inboundNodeQueue.push(node);
        outboundNodeQueue.push(node);
        resultNodeSet.add(node);
      }
    }

    /**
     * Runs BFS and updates the result (resultNodeSet, resultEdgeSet).
     *
     * @param {boolean} inboundTraversal Whether inbound edges should be used to
     *     traverse. If false, outbound edges are used.
     * @param {!Set<string>} seenNodes The IDs of nodes already visited in the
     *     BFS. Will be modified.
     * @param {!Array<!GraphNode>} nodeQueue The queue used in BFS. Will be
     *     modified.
     * @param {number} maxDepth The depth of the traversal.
     */
    const updateResultBFS = (
        inboundTraversal, seenNodes, nodeQueue, maxDepth) => {
      while (nodeQueue.length > 0) {
        // The performance on the repeated `shift()`s is not as bad as it might
        // seem, since we only have ~500 nodes for the package graph.
        const curNode = nodeQueue.shift();
        const curDepth = inboundTraversal ?
          curNode.visualizationState.inboundDepth :
          curNode.visualizationState.outboundDepth;
        if (curDepth < maxDepth) {
          const otherNodes = inboundTraversal ?
            curNode.inbound : curNode.outbound;
          for (const otherNode of otherNodes) {
            if (!seenNodes.has(otherNode.id)) {
              if (inboundTraversal) {
                otherNode.visualizationState.selectedByInbound = true;
                otherNode.visualizationState.inboundDepth = curDepth + 1;
              } else {
                otherNode.visualizationState.selectedByOutbound = true;
                otherNode.visualizationState.outboundDepth = curDepth + 1;
              }
              nodeQueue.push(otherNode);
              seenNodes.add(otherNode.id);
              resultNodeSet.add(otherNode);
            }
            const edgeTraversedId = inboundTraversal ?
              getEdgeIdFromNodes(otherNode.id, curNode.id) :
              getEdgeIdFromNodes(curNode.id, otherNode.id);
            resultEdgeSet.add(this.getEdgeById(edgeTraversedId));
          }
        }
      }
    };

    updateResultBFS(/* inboundTraversal */ true, inboundSeenNodes,
        inboundNodeQueue, inboundDepth);
    updateResultBFS(/* inboundTraversal */ false, outboundSeenNodes,
        outboundNodeQueue, outboundDepth);

    // Special case: If inbound and outbound are both 0, both BFS will be a
    // no-op and the edges between filtered nodes will not be included. In this
    // case, we include those edges manually.
    if (inboundDepth === 0 && outboundDepth === 0) {
      for (const filterNode of resultNodeSet) {
        for (const otherNode of filterNode.inbound) {
          if (resultNodeSet.has(otherNode)) {
            resultEdgeSet.add(this.getEdgeById(
                getEdgeIdFromNodes(otherNode.id, filterNode.id)));
          }
        }
        for (const otherNode of filterNode.outbound) {
          if (resultNodeSet.has(otherNode)) {
            resultEdgeSet.add(this.getEdgeById(
                getEdgeIdFromNodes(filterNode.id, otherNode.id)));
          }
        }
      }
    }

    return {
      nodes: [...resultNodeSet],
      edges: [...resultEdgeSet],
    };
  }
}

export {
  ClassNode,
  D3GraphData,
  GraphEdge,
  GraphModel,
  GraphNode,
  PackageNode,
  TargetNode,
};
