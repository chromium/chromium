// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @file Exports a class GraphView, which encapsulates all the D3 logic
 * in the graph visualization.
 *
 * D3 uses the concept of "data joins" to bind data to the SVG, and might be
 * difficult to read at first glance. This (https://bost.ocks.org/mike/join/) is
 * a good resource if you are unfamiliar with data joins.
 */

import {DisplaySettingsData} from './display_settings_data.js';
import {GraphNode, D3GraphData} from './graph_model.js';
import {GraphEdgeColor} from './display_settings_data.js';

import * as d3 from 'd3';

// The radius of each node in the visualization.
const NODE_RADIUS = 5;

// Category10 colors pulled from https://observablehq.com/@d3/color-schemes.
const HULL_COLORS = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd',
  '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf'];
// The distance the hull should be expanded from the original nodes.
const HULL_EXPANSION = 20;

// The perpendicular distance from the center of an edge to place the control
// point for a quadratic Bezier curve.
const EDGE_CURVE_OFFSET = {
  CURVED: 30,
  STRAIGHT: 0,
};

// The default color for edges.
const DEFAULT_EDGE_COLOR = '#999';

// A map from GraphEdgeColor to its start and end color codes. The property
// `targetDefId` will be used as the unique ID of the colored arrow in the SVG
// defs, and the arrowhead will be referred to by `url(#targetDefId)` in the
// SVG.
const EDGE_COLORS = {
  [GraphEdgeColor.DEFAULT]: {
    source: DEFAULT_EDGE_COLOR,
    target: DEFAULT_EDGE_COLOR,
    targetDefId: 'graph-arrowhead-default',
  },
  [GraphEdgeColor.GREY_GRADIENT]: {
    source: '#ddd',
    target: '#666',
    targetDefId: 'graph-arrowhead-grey-gradient',
  },
  [GraphEdgeColor.BLUE_TO_RED]: {
    source: '#00f',
    target: '#f00',
    targetDefId: 'graph-arrowhead-blue-to-red',
  },
};

// Parameters determining the force-directed simulation cooldown speed.
// For more info on each param, see https://github.com/d3/d3-force.
const SIMULATION_SPEED_PARAMS = {
  // Alpha (or temperature, as in simulated annealing) is reset to this value
  // for the simulation is reheated.
  ALPHA_ON_REHEAT: 0.3,

  // Alpha at which simulation freezes.
  ALPHA_MIN: 0.001,

  // A higher velocity decay indicates slower nodes (node movement is multiplied
  // by (1-decay) each tick).

  // The decay to use when there is no easing (eg. dragging nodes).
  VELOCITY_DECAY_DEFAULT: 0.8,

  // When nodes are eased, their decay starts at MAX (slow),
  // transitions to MIN (fast), then back to MAX.
  VELOCITY_DECAY_MAX: 0.99,
  VELOCITY_DECAY_MIN: 0,
};

// Colors for displayed nodes. These colors are temporary.
const NODE_COLORS = {
  FILTER: d3.color('red'),
  INBOUND: d3.color('#000080'), // Dark blue.
  OUTBOUND: d3.color('#666600'), // Dark yellow.
  INBOUND_AND_OUTBOUND: d3.color('#006400'), // Dark green.
};

/**
 * Computes the color to display for a given node.
 *
 * @param {!GraphNode} node The node in question.
 * @return {string} The color of the node.
 */
function getNodeColor(node) {
  if (node.visualizationState.selectedByFilter) {
    return NODE_COLORS.FILTER;
  }
  if (node.visualizationState.selectedByInbound &&
    node.visualizationState.selectedByOutbound) {
    return NODE_COLORS.INBOUND_AND_OUTBOUND.brighter(
        Math.min(node.visualizationState.inboundDepth,
            node.visualizationState.outboundDepth));
  }
  if (node.visualizationState.selectedByInbound) {
    return NODE_COLORS.INBOUND.brighter(
        node.visualizationState.inboundDepth);
  }
  if (node.visualizationState.selectedByOutbound) {
    return NODE_COLORS.OUTBOUND.brighter(
        node.visualizationState.outboundDepth);
  }
  throw new Error(`Unexpected node visualization state ${node}`);
}

/**
 * Adds a def for an arrowhead (triangle) marker to the SVG.
 *
 * @param {*} defs The d3 selection of the SVG defs.
 * @param {string} id The HTML id for the arrowhead.
 * @param {string} color The color of the arrowhead.
 * @param {number} length The length of the arrowhead.
 * @param {number} width The width of the arrowhead.
 */
function addArrowMarkerDef(defs, id, color, length, width) {
  const halfWidth = Math.floor(width / 2);
  defs.append('marker')
      .attr('id', id) // 'graph-arrowhead-*'
      .attr('viewBox', `0 -${halfWidth} ${length} ${width}`)
      .attr('refX', length + NODE_RADIUS)
      .attr('refY', 0)
      .attr('orient', 'auto')
      .attr('markerWidth', length)
      .attr('markerHeight', width)
      .append('path')
      .attr('d', `M 0 -${halfWidth} L ${length} 0 L 0 ${halfWidth}`)
      .attr('fill', color)
      .style('stroke', 'none');
}

/**
 * Creates a polygon (array of [x, y] pairs) that can be converted into a
 * valid convex hull. For a polygon to be valid for conversion, it must have
 * at least 3 points.
 *
 * @param {!Array<!GraphNode>} nodes The nodes to generate a polygon for. This
 *     array must have at least one element.
 * @return {!Array<!Array<number>>} A valid polygon for the input nodes.
 */
function getValidHullPolygon(nodes) {
  let nodePolygon = nodes.map(node => [node.x, node.y]);
  if (nodePolygon.length === 1) {
    // If there is only one point, replace it with four points in a square
    // diamond surrounding the original point.
    const [x0, y0] = nodePolygon[0];
    nodePolygon = [[x0 + 1, y0], [x0 - 1, y0], [x0, y0 + 1], [x0, y0 - 1]];
  } else if (nodePolygon.length === 2) {
    // If there are two points, add another two slightly offset points
    // parallel to the line created by the original two points.
    const [x0, y0] = nodePolygon[0];
    const [x1, y1] = nodePolygon[1];
    const deltaX = x1 - x0;
    const deltaY = y1 - y0;
    const [offsetX, offsetY] = resizeVector(deltaX, deltaY, 1);
    /* eslint-disable indent */
    // Here, we rotate the offset vector 90deg by negating x and swapping x, y.
    nodePolygon.push([x0 + offsetY, y0 - offsetX],
                     [x1 + offsetY, y1 - offsetX]);
  }
  // If there are >= 3 points, it is already valid for convex hull conversion.
  return nodePolygon;
}

/**
 * When we reheat the simulation, we'd like to know how many ticks it will take
 * to cool. Instead of finding an formula for our specific config, we just make
 * a throwaway simulation with our config and run it to completion.
 *
 * @return {number} The number of ticks it will take for a reheat to cool.
 */
function countNumReheatTicks() {
  let reheatTicks = 0;
  const reheatTicksCounter = d3.forceSimulation()
      .alphaMin(SIMULATION_SPEED_PARAMS.ALPHA_MIN)
      .alpha(SIMULATION_SPEED_PARAMS.ALPHA_ON_REHEAT);
  while (reheatTicksCounter.alpha() > reheatTicksCounter.alphaMin()) {
    reheatTicks++;
    reheatTicksCounter.tick();
  }
  return reheatTicks;
}

/**
 * Helper to scale a vector to a given magnitude. If [x, y] is nearly the zero
 * vector, then [newMagnitude, 0] is returned.
 *
 * @param {number} x The x-coordinate of the vector.
 * @param {number} y The y-coordinate of the vector.
 * @param {number} newMagnitude The magnitude to scale the vector to.
 * @return {!Array<number>} The scaled vector in the form [x, y].
 */
function resizeVector(x, y, newMagnitude) {
  const normalFactor = Math.hypot(x, y);
  if (normalFactor < 1e-12) {
    return [newMagnitude, 0];
  }
  const scaleFactor = newMagnitude / normalFactor;
  return [x * scaleFactor, y * scaleFactor];
}

/**
 * Wrapper class around the logic for the currently hovered node.
 *
 * The hovered node should not change when being dragged, even if the "real"
 * hovered node (as determined by mouse position) changes during the drag (e.g.,
 * if the mouse moves too fast). Update takes place when drag ends, i.e., the
 * dragged node become unhovered after drag if the mouse came off the node.
 */
class HoveredNodeManager {
  constructor() {
    /** @public {?GraphNode} */
    this.hoveredNode = null;
    /** @private {?GraphNode} */
    this.realHoveredNode_ = null;
    /** @private {boolean} */
    this.isDragging_ = false;
  }
  setDragging(isDragging) {
    this.isDragging_ = isDragging;
    if (!this.isDragging_) {
      // When the drag ends, update the hovered node.
      this.hoveredNode = this.realHoveredNode_;
    }
  }
  setHoveredNode(hoveredNode) {
    this.realHoveredNode_ = hoveredNode;
    if (!this.isDragging_) {
      // The hovered node can only be updated when not dragging.
      this.hoveredNode = this.realHoveredNode_;
    }
  }
}

/**
 * Wrapper class around the logic to assign colors to convex hulls.
 *
 * Each hull should have a unique key that remains associated with the hull
 * throughout its lifetime.
 */
class HullColorManager {
  constructor() {
    this.colorIndex_ = 0;
    this.hullColors_ = new Map();
  }
  /**
   * Gets the next color from the array of colors `HULL_COLORS`. When all the
   * colors of the array are used, starts from the beginning again.
   *
   * @return {string} The next color to use.
   */
  getNextColor_() {
    this.colorIndex_ = (this.colorIndex_ + 1) % HULL_COLORS.length;
    return HULL_COLORS[this.colorIndex_];
  }

  /**
   * Gets the color associated with a given hull, generating a color for it if
   * there isn't one already.
   *
   * @param {string} hullKey A key uniquely identifying the hull.
   * @return {string} The color associated with the hull's key.
   */
  getColorForHull(hullKey) {
    if (!this.hullColors_.has(hullKey)) {
      this.hullColors_.set(hullKey, this.getNextColor_());
    }
    return this.hullColors_.get(hullKey);
  }
}

/**
 * @typedef {object} PhantomTextNode A node that isn't displayed in the
 *     visualization, but affects the simulation. Used to prevent text overlap
 *     by being fixed to the right of real nodes (where the text is drawn).
 * @property {boolean} isPhantomTextNode A flag (always true) used to
 *     differentiate these nodes from real ones when processing the simulation.
 * @property {!GraphNode} refNode A reference to the node that this phantom node
 *     is attached to.
 * @property {number} dist The distance away from `refNode` that this node
 *     should be fixed.
 */

/**
 * A callback to be triggered whenever a node is clicked in the visualization.
 *
 * @callback OnNodeClickedCallback
 * @param {!GraphNode} node The node that was clicked.
 */

/**
 * A callback to be triggered whenever a node is double-clicked in the
 * visualization.
 *
 * @callback OnNodeDoubleClickedCallback
 * @param {!GraphNode} node The node that was double-clicked.
 */

/**
 * Returns the group a node is in, or `null` if the node shouldn't be grouped.
 *
 * @callback GetNodeGroupCallback
 * @param {!GraphNode} node The node to find the group for.
 * @return {?string} The unique key identifying the node's group, or `null` if
 *     the node shouldn't be grouped.
 */

/** The view of the visualization, controlling display on the SVG. */
class GraphView {
  /**
   * Initializes some variables and performs one-time setup of the SVG canvas.
   * Currently just binds to the only 'svg' object, as things get more complex
   * we can maybe change this to bind to a given DOM element if necessary.
   */
  constructor() {
    /** @private {number} */
    this.edgeCurveOffset_ = EDGE_CURVE_OFFSET.CURVED;
    /** @private {boolean} */
    this.colorEdgesOnlyOnHover_ = true;
    /** @private {string} */
    this.graphEdgeColor_ = GraphEdgeColor.DEFAULT;
    /** @private {boolean} */
    this.reheatRequested_ = false;
    /** @private {?string} */
    this.lastHullDisplay_ = null;
    /** @private {!HoveredNodeManager} */
    this.hoveredNodeManager_ = new HoveredNodeManager();
    /** @private {!HullColorManager} */
    this.hullColorManager_ = new HullColorManager();

    // Event handler callbacks, to be registered externally and called when the
    // relevant event is triggered.
    /** @private @type {?GetNodeGroupCallback} */
    this.getNodeGroup_ = null;
    /** @private @type {?OnNodeClickedCallback} */
    this.onNodeClicked_ = null;
    /** @private @type {?OnNodeDoubleClickedCallback} */
    this.onNodeDoubleClicked_ = null;

    const svg = d3.select('#graph-svg');
    // An SVG group containing the entire graph as its children (for zoom/pan).
    const graphGroup = svg.append('g');
    // The defs element for the entire SVG
    // (https://developer.mozilla.org/en-US/docs/Web/SVG/Element/defs).
    this.svgDefs_ = svg.append('defs');

    // Add an arrowhead def for every possible edge target color.
    for (const {target, targetDefId} of Object.values(EDGE_COLORS)) {
      addArrowMarkerDef(this.svgDefs_, targetDefId, target, 10, 6);
    }

    // Set up zoom and pan on the entire graph.
    svg.call(d3.zoom()
        .scaleExtent([0.25, 10])
        .on('zoom', event =>
          graphGroup.attr('transform', event.transform),
        ))
        .on('dblclick.zoom', null);

    // Each group here will have a collection of SVG elements as their children.
    // The order of these groups decide the SVG paint order (since we append
    // sequentially), earlier groups will be painted below later ones.
    /**
     * @private {*} The convex hulls displayed around node groupings. Contains
     * <path/> elements as children, each hull is one <path/>.
     */
    this.hullGroup_ = graphGroup.append('g')
        .classed('graph-hull', true)
        .attr('stroke-width', 1)
        .attr('fill-opacity', 0.1);
    /**
     * @private {*} The edges of the graph. Contains <path/> elements as
     * children, each edge is one <path/>.
     */
    this.edgeGroup_ = graphGroup.append('g')
        .classed('graph-edges', true)
        .attr('stroke-width', 1)
        .attr('fill', 'transparent');
    /**
     * @private {*} The nodes of the graph. Contains <circle/> elements as
     * children, each node is one <circle/>.
     */
    this.nodeGroup_ = graphGroup.append('g')
        .classed('graph-nodes', true);
    /**
     * @private {*} The labels for the convex hulls contained in
     * `this.hullGroup_`. Contains <text/> elements as children, each hull label
     * is one <text/>.
     */
    this.hullLabelGroup_ = graphGroup.append('g')
        .classed('graph-hull-labels', true)
        .attr('pointer-events', 'none');
    /**
     * @private {*} The labels for the nodes in `this.nodeGroup_`. Contains
     * <text/> elements as children, each node label is one <text/>.
     */
    this.labelGroup_ = graphGroup.append('g')
        .classed('graph-labels', true)
        .attr('pointer-events', 'none');

    /** @private {!Array<!PhantomTextNode>} */
    this.phantomTextNodes_ = [];

    // Using .style() instead of .attr() gets px-based measurements of
    // percentage-based widths and heights.
    const width = parseInt(svg.style('width'), 10);
    const height = parseInt(svg.style('height'), 10);

    // Initializes forces in the force-directed simulation, which will update
    // position variables on the input data as the simulation runs.
    const centeringStrengthY = 0.07;
    const centeringStrengthX = centeringStrengthY * (height / width);
    /** @private {*} */
    this.simulation_ = d3.forceSimulation()
        .alphaMin(SIMULATION_SPEED_PARAMS.ALPHA_MIN)
        .force('chargeForce', d3.forceManyBody().strength(node => {
            if (node.isPhantomTextNode) {
              return -1100;
            }
            return -3000;
        }))
        .force('centerXForce',
            d3.forceX(width / 2).strength(node => {
              if (node.isPhantomTextNode) {
                return 0;
              }
              if (node.visualizationState.selectedByFilter) {
                return centeringStrengthX * 15;
              }
              if (node.visualizationState.outboundDepth <= 1) {
                return centeringStrengthX * 5;
              }
              return centeringStrengthY;
            }))
        .force('centerYForce',
            d3.forceY(height / 2).strength(node => {
              if (node.isPhantomTextNode) {
                return 0;
              }
              if (node.visualizationState.selectedByFilter) {
                return centeringStrengthY * 15;
              }
              if (node.visualizationState.outboundDepth <= 1) {
                return centeringStrengthY * 5;
              }
              return centeringStrengthY;
            }));

    /** @private {number} */
    this.reheatTicks_ = countNumReheatTicks();

    /**
     * @callback LinearScaler
     * @param {number} input The input value between [0, 1] inclusive.
     * @return {number} The input scaled linearly to new bounds.
     */
    /** @private {LinearScaler} */
    this.velocityDecayScale_ = d3.scaleLinear()
        .domain([0, 1])
        .range([SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_MIN,
          SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_MAX]);
  }

  /**
   * Binds the event when a node is clicked in the graph to a given callback.
   *
   * @param {!OnNodeClickedCallback} onNodeClicked The callback to bind to.
   */
  registerOnNodeClicked(onNodeClicked) {
    this.onNodeClicked_ = onNodeClicked;
  }

  /**
   * Binds the event when a node is double-clicked in the graph to a given
   * callback.
   *
   * @param {!OnNodeDoubleClickedCallback} onNodeDoubleClicked The callback to
   *   bind to.
   */
  registerOnNodeDoubleClicked(onNodeDoubleClicked) {
    this.onNodeDoubleClicked_ = onNodeDoubleClicked;
  }

  /**
   * Assigns the node group accessor to a given function.
   *
   * @param {!GetNodeGroupCallback} getNodeGroup The function to assign to.
   */
  registerGetNodeGroup(getNodeGroup) {
    this.getNodeGroup_ = getNodeGroup;
  }

  /**
   * Computes the velocityDecay for our current progress in the reheat process.
   *
   * See https://github.com/d3/d3-force#simulation_velocityDecay. We animate new
   * nodes on the page by starting off with a high decay (slower nodes), easing
   * to a low decay (faster nodes), then easing back to a high decay at the end.
   * This makes the node animation seem more smooth and natural.
   *
   * @param {number} currentTick The number of ticks passed in the reheat.
   * @return {number} The velocityDecay for the current point in the reheat.
   */
  getEasedVelocityDecay(currentTick) {
    // The input to the ease function has to be in [0, 1] inclusive. Since
    // velocity is multiplied by (1 - decay) and we want speeds of
    // slow-fast-slow, the midpoint of the process should correspond to 0 and
    // the beginning and end to 1 (ie. the decay should look like \/).
    const normalizedCurrentTick = Math.abs(
        1 - 2 * (currentTick / this.reheatTicks_));
    const normalizedEaseTick = d3.easeQuadInOut(normalizedCurrentTick);
    // Since the ease returns a value in [0, 1] inclusive, we have to scale it
    // back to our desired velocityDecay range before returning.
    return (this.velocityDecayScale_(normalizedEaseTick));
  }

  /** Synchronizes the path of all edges to their underlying data. */
  syncEdgePaths() {
    this.edgeGroup_.selectAll('path')
        .attr('d', edge => {
          // To calculate the control point, consider the edge vector [dX, dY]:
          // * Flip over Y axis (since SVGs increase y downwards): [dX, -dY]
          // * Rotate 90 degrees clockwise: [-dY, -dX]
          // * Flip over Y again to get SVG coords: [-dY, dX]
          // * Scale and add to midpoint: [midX, midY] + scaleFactor * [-dY, dX]
          //   where `scaleFactor` rescales [-dY, dX] to have the length of
          //   `this.edgeCurveOffset_`.
          const deltaX = edge.target.x - edge.source.x;
          const deltaY = edge.target.y - edge.source.y;
          if (deltaX === 0 && deltaY === 0) {
            return null; // Do not draw paths for self-edges.
          }

          const midX = (edge.source.x + edge.target.x) / 2;
          const midY = (edge.source.y + edge.target.y) / 2;

          const [offsetX, offsetY] = resizeVector(
              deltaX, deltaY, this.edgeCurveOffset_);
          const controlPointX = midX - offsetY;
          const controlPointY = midY + offsetX;

          const path = d3.path();
          path.moveTo(edge.source.x, edge.source.y);
          path.quadraticCurveTo(
              controlPointX, controlPointY, edge.target.x, edge.target.y);
          return path.toString();
        });
  }

  /** Synchronizes the color of all edges to their underlying data. */
  syncEdgeColors() {
    const hoveredNode = this.hoveredNodeManager_.hoveredNode;
    const edgeTouchesHoveredNode = edge =>
      edge.source === hoveredNode || edge.target === hoveredNode;
    const nodeTouchesHoveredNode = node => {
      return node === hoveredNode ||
        hoveredNode.inbound.has(node) || hoveredNode.outbound.has(node);
    };

    // Point the associated gradient in the direction of the line.
    this.svgDefs_.selectAll('linearGradient')
        .attr('x1', edge => edge.source.x)
        .attr('y1', edge => edge.source.y)
        .attr('x2', edge => edge.target.x)
        .attr('y2', edge => edge.target.y);

    this.edgeGroup_.selectAll('path')
        .attr('marker-end', edge => {
          if (edge.source === edge.target) {
            return null;
          } else if (!this.colorEdgesOnlyOnHover_ ||
              edgeTouchesHoveredNode(edge)) {
            return `url(#${EDGE_COLORS[this.graphEdgeColor_].targetDefId})`;
          }
          return `url(#${EDGE_COLORS[GraphEdgeColor.DEFAULT].targetDefId})`;
        })
        .attr('stroke', edge => {
          if (!this.colorEdgesOnlyOnHover_ || edgeTouchesHoveredNode(edge)) {
            return `url(#${edge.id})`;
          }
          return DEFAULT_EDGE_COLOR;
        })
        .classed('non-hovered-edge', edge => {
          return this.colorEdgesOnlyOnHover_ &&
            hoveredNode !== null && !edgeTouchesHoveredNode(edge);
        });

    this.labelGroup_.selectAll('text')
        .classed('non-hovered-text', node => {
          return this.colorEdgesOnlyOnHover_ &&
            hoveredNode !== null && !nodeTouchesHoveredNode(node);
        });
  }

  /** Updates the colors of the edge gradients to match the selected color. */
  syncEdgeGradients() {
    const {source, target} = EDGE_COLORS[this.graphEdgeColor_];
    const gradientSelection = this.svgDefs_.selectAll('linearGradient');
    gradientSelection.selectAll('stop').remove();
    gradientSelection.append('stop')
        .attr('offset', '0%')
        .attr('stop-color', source);
    gradientSelection.append('stop')
        .attr('offset', '100%')
        .attr('stop-color', target);
  }

  /**
   * Groups nodes together by using the current group accessor function.
   *
   * @param {!Array<!GraphNode>} nodes The nodes to group.
   * @return {!Map<string, !Array<!GraphNode>>} The map from group key to the
   *     list of nodes included in that group.
   */
  getNodeGroups(nodes) {
    const groups = new Map();
    for (const node of nodes) {
      const groupKey = this.getNodeGroup_(node);
      // If the key is null, the node should not be grouped.
      if (groupKey !== null) {
        if (!groups.has(groupKey)) {
          groups.set(groupKey, [node]);
        } else {
          groups.get(groupKey).push(node);
        }
      }
    }
    return groups;
  }

  /**
   * Data representing a convex hull surrounding a certain group.
   *
   * @typedef {object} HullData
   * @property {string} key The unique key for the hull.
   * @property {string} color The color to display the hull as.
   * @property {!Array<number>} labelPosition An [x, y] point representing where
   *     this hull's label should be rendered.
   * @property {!Array<!Array<number>>} points A list of [x, y] points making up
   *     the hull.
   */

  /**
   * Given the node grouping from `getNodeGroups`, constructs a list of convex
   * hulls, one per node group.
   *
   * @param {!Map<string, !Array<!GraphNode>>} nodeGroups The node groupings.
   * @return {!Array<!HullData>} A list of convex hulls to display.
   */
  getConvexHullData(nodeGroups) {
    const resultHulls = [];
    for (const [key, nodes] of nodeGroups.entries()) {
      const nodePolygon = getValidHullPolygon(nodes);
      const baseHull = d3.polygonHull(nodePolygon);
      // To expand the hull, we move each point a set distance away from the
      // hull's centroid (https://en.wikipedia.org/wiki/Centroid).
      const [centroidX, centroidY] = d3.polygonCentroid(baseHull);
      const expandedPolygon = baseHull.map(([nodeX, nodeY]) => {
        const deltaX = nodeX - centroidX;
        const deltaY = nodeY - centroidY;
        const [expandX, expandY] = resizeVector(deltaX, deltaY, HULL_EXPANSION);
        return [nodeX + expandX, nodeY + expandY];
      });
      // A decent heuristic is for the hull's label to be displayed above its
      // highest node. Recall that on an SVG, lower y-values are higher.
      let highestPoint = [0, Number.POSITIVE_INFINITY];
      for (const point of expandedPolygon) {
        if (point[1] < highestPoint[1]) {
          highestPoint = point;
        }
      }
      resultHulls.push({
        key,
        color: this.hullColorManager_.getColorForHull(key),
        labelPosition: highestPoint,
        points: expandedPolygon,
      });
    }
    return resultHulls;
  }

  /**
   * Synchronizes the color and position of all convex hulls with their
   * underlying data.
   *
   * @param {!Array<!HullData>} hullData A list of convex hulls to display for
   *     the current data.
   */
  updateHullData(hullData) {
    this.hullLabelGroup_.selectAll('text')
      .data(hullData, hull => hull.key)
      .join(enter => enter.append('text')
          .text(hull => hull.key)
          .attr('fill', hull => hull.color)
          .attr('dy', -8)
          .attr('x', hull => hull.labelPosition[0])
          .attr('y', hull => hull.labelPosition[1]),
        update => update
          .attr('x', hull => hull.labelPosition[0])
          .attr('y', hull => hull.labelPosition[1]));

    // The SVG path generator for the hull outlines.
    const getHullLine = d3.line().curve(d3.curveCatmullRomClosed.alpha(0.75));
    this.hullGroup_.selectAll('path')
        .data(hullData, hull => hull.key)
        .join(enter => enter.append('path')
            .attr('d', hull => getHullLine(hull.points))
            .attr('stroke', hull => hull.color)
            .attr('fill', hull => hull.color),
        update => update.attr('d', hull => getHullLine(hull.points)));
  }

  /**
   * Reheats the simulation, allowing all nodes to move according to the physics
   * simulation until they cool down again.
   *
   * @param {boolean} shouldEase Whether the node movement should be eased. This
   *     should not be used when dragging nodes, since the speed at the start of
   *     the ease will be used all throughout the drag.
   */
  reheatSimulation(shouldEase) {
    let tickNum = 0;

    /**
     * Executed every time the simulation updates.
     *
     * Every tick of the simulation, we need to manually sync the visualization
     * with the updated position variables. For more info on the position
     * variables, see (https://github.com/d3/d3-force#simulation_nodes).
     *
     * Any part of the visualization that need to update on tick (e.g., node
     * positions) should have their updates be performed in here.
     */
    const tickActions = () => {
      this.syncEdgePaths();
      this.syncEdgeColors();

      this.nodeGroup_.selectAll('circle')
          .attr('cx', node => node.x)
          .attr('cy', node => node.y);

      this.labelGroup_.selectAll('text')
          .attr('x', label => label.x)
          .attr('y', label => label.y);

      const hullData = this.getConvexHullData(
          this.getNodeGroups(this.nodeGroup_.selectAll('circle').data()));
      this.updateHullData(hullData);

      tickNum ++;
      if (shouldEase) {
        this.simulation_.velocityDecay(this.getEasedVelocityDecay(tickNum));
      }

      // Reset phantom nodes to their fixed positions
      this.phantomTextNodes_.forEach(phantomNode => {
        phantomNode.x = phantomNode.refNode.x + phantomNode.dist;
        phantomNode.y = phantomNode.refNode.y;
      });
    };

    // If we don't ease, the default decay is sufficient for the entire reheat.
    const startingVelocityDecay = shouldEase ?
      this.getEasedVelocityDecay(0) :
      SIMULATION_SPEED_PARAMS.VELOCITY_DECAY_DEFAULT;

    this.simulation_
        .on('tick', tickActions)
        .velocityDecay(startingVelocityDecay)
        .alpha(SIMULATION_SPEED_PARAMS.ALPHA_ON_REHEAT)
        .restart();
  }

  /**
   * Updates the display settings for the visualization.
   *
   * @param {!DisplaySettingsData} displaySettings The display config.
   */
  updateDisplaySettings(displaySettings) {
    const {
      curveEdges,
      colorOnlyOnHover,
      graphEdgeColor,
    } = displaySettings;
    this.edgeCurveOffset_ = curveEdges ?
      EDGE_CURVE_OFFSET.CURVED : EDGE_CURVE_OFFSET.STRAIGHT;
    this.colorEdgesOnlyOnHover_ = colorOnlyOnHover;
    this.graphEdgeColor_ = graphEdgeColor;

    // Reheat if node grouping changed.
    if (this.lastHullDisplay_ !== displaySettings.hullDisplay) {
      this.lastHullDisplay_ = displaySettings.hullDisplay;
      this.reheatRequested_ = true;
    }

    this.syncEdgeGradients();
    this.syncEdgePaths();
    this.syncEdgeColors();
  }

  /**
   * Generates sparse integers subset in [0, n] that's roughly evenly
   * distributed.
   *
   * @generator
   * @param {number} upperBound Exclusive upper bound on generated values.
   * @param {number} separation Ideal separation between generated values.
   * @yields {number} Generated integers.
   */
   *generateSparseInts(upperBound, separation) {
    for (let i = separation; i < upperBound; i += separation) {
      yield i;
    }
    // Add an endpoint if the upper bound is far from the last value generated.
    if (upperBound % separation >= separation / 2) {
      yield upperBound - 1;
    }
  }

  /**
   * Updates the data source used for the visualization.
   *
   * @param {!D3GraphData} inputData The new data to use.
   */
  updateGraphData(inputData) {
    const DIST_MULTIPLIER = 6.6;
    const {nodes: inputNodes, edges: inputEdges} = inputData;

    this.phantomTextNodes_ = [];
    // Generate the phantom text nodes, the list of nodes to be included in the
    // physics simulation at the place where node labels will be rendered.
    for (const node of inputNodes) {
      // A heuristic in the absence of exact label width: place phantom nodes
      // approximately every 10 characters away from the real node.
      for (const pos of this.generateSparseInts(node.displayName.length, 10)) {
        this.phantomTextNodes_.push({
          isPhantomTextNode: true,
          refNode: node,
          dist: pos * DIST_MULTIPLIER,
        });
      }
    }

    this.simulation_
        .nodes([...inputNodes, ...this.phantomTextNodes_])
        .force('links', d3.forceLink(inputEdges).id(edge => edge.id));

    let nodesAddedOrRemoved = false;
    this.svgDefs_.selectAll('linearGradient')
        .data(inputEdges, edge => edge.id)
        .join(enter => enter.append('linearGradient')
            .attr('id', edge => edge.id)
            .attr('gradientUnits', 'userSpaceOnUse'));

    this.edgeGroup_.selectAll('path')
        .data(inputEdges, edge => edge.id)
        .join(enter => enter.append('path'));

    this.nodeGroup_.selectAll('circle')
        .data(inputNodes, node => node.id)
        .join(enter => {
          if (!enter.empty()) {
            nodesAddedOrRemoved = true;
          }
          const graphView = this;
          return enter.append('circle')
              .attr('r', 5)
              .attr('stroke', node => getNodeColor(node))
              .on('dblclick', (_, node) => this.onNodeDoubleClicked_(node))
              .on('mousedown', (_, node) => this.onNodeClicked_(node))
              .on('mouseenter', (_, node) => {
                this.hoveredNodeManager_.setHoveredNode(node);
                this.syncEdgeColors();
              })
              .on('mouseleave', () => {
                this.hoveredNodeManager_.setHoveredNode(null);
                this.syncEdgeColors();
              })
              .call(d3.drag()
                  .on('start', () => this.hoveredNodeManager_.setDragging(true))
                  // It is necessary to use function instead of => to capture
                  // the actual circle element in 'this' for d3 v6+.
                  .on('drag', function(event, node) {
                    graphView.reheatSimulation(/* shouldEase */ false);
                    // eslint-disable-next-line no-invalid-this
                    d3.select(this).classed('locked', true);
                    // Fix the node's position after it has been dragged.
                    node.fx = event.x;
                    node.fy = event.y;
                  })
                  .on('end', () => this.hoveredNodeManager_.setDragging(false)))
              // It is necessary to use function instead of => to capture
              // the actual circle element in 'this' for d3 v6+.
              .on('click', function(event, node) {
                if (event.defaultPrevented) {
                  return; // Skip drag events.
                }
                // eslint-disable-next-line no-invalid-this
                const pageNode = d3.select(this);
                if (pageNode.classed('locked')) {
                  node.fx = null;
                  node.fy = null;
                  graphView.reheatSimulation(/* shouldEase */ true);
                } else {
                  node.fx = node.x;
                  node.fy = node.y;
                }
                pageNode.classed('locked', !pageNode.classed('locked'));
              });
        },
          update => update.attr('stroke', node => getNodeColor(node)),
          exit => {
            if (!exit.empty()) {
              nodesAddedOrRemoved = true;
            }
            // When a node is removed from the SVG, it should lose all
            // position-related data.
            return exit.each(node => {
              node.x = null;
              node.y = null;
              node.fx = null;
              node.fy = null;
            }).remove();
          })
        .attr('fill', node => {
          const nodeGroup = this.getNodeGroup_(node);
          if (nodeGroup === null) {
            return '#fff';
          }
          return this.hullColorManager_.getColorForHull(nodeGroup);
        });

    const nodeGroupings = this.getNodeGroups(inputNodes);
    this.updateHullData(this.getConvexHullData(nodeGroupings));

    // Create a link force between every pair of nodes in a group, causing the
    // nodes to group together in the visualization.
    for (const [key, nodes] of nodeGroupings.entries()) {
      const links = [];
      // If performance is an issue, this can be replaced by picking a
      // representative node and connecting all other group members to it. This
      // has the downside of inconsistent node interactions: Dragging the
      // representative node pulls the entire group with it, while dragging
      // non-representative nodes doesn't affect the group as much.
      for (let sourceIndex = 0; sourceIndex < nodes.length; sourceIndex++) {
        for (let targetIndex = sourceIndex + 1;
          targetIndex < nodes.length; targetIndex++) {
          links.push({
            source: nodes[sourceIndex],
            target: nodes[targetIndex],
          });
        }
      }
      this.simulation_.force(key, d3.forceLink(links));
    }

    this.labelGroup_.selectAll('text')
        .data(inputNodes, node => node.id)
        .join(enter => enter.append('text')
            .attr('dx', 12)
            .attr('dy', '.35em')
            .text(label => label.displayName));

    // The graph should not be reheated on a no-op (eg. adding a visible node to
    // the filter which doesn't add/remove any new nodes).
    if (this.reheatRequested_ || nodesAddedOrRemoved) {
      this.reheatRequested_ = false;
      this.simulation_.stop();
      this.reheatSimulation(/* shouldEase */ true);
    }
  }
}

export {
  GraphView,
};
