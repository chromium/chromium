// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and helpers for viewing and interacting with the Metrics Tree.
 */

/**
 * @typedef {Object} MetricsItem
 * @property {string} name - Item name, used for UI and grouping for total.
 * @property {number} value - Metrics value, can be bytes or count.
 * @property {number|undefined} beforeValue - Optional value for "before".
 */

/**
 * @typedef {Object} MetricsTreeNode
 * @property {string|undefined} name - The full name of the node, and is shown
 *     in the UI.
 * @property {!Array<!MetricsTreeNode>|undefined} children - Child nodes.
 *     Non-existent or null indicates this is a leaf node.
 * @property {!Array<!MetricsTreeNode>|undefined} totals - An optional list of
 *     special nodes (not in the tree via |children|). These nodes' |children|
 *     stores a list of existing source leaves (in the tree), whose |items| are
 *     to be summed to compute the desired total, subject to filtering.
 * @property {!Array<!MetricsItem>|undefined} items - For leaf nodes only, a
 *     list of named values for a metric.
 * @property {boolean|undefined} isFiltered - For group nodes only, whether
 *     filtering is applied to the node.
 * @property {string|undefined} iconKey - For group nodes only, input for
 *     getMetricsIconTemplate() to retrieve icon.
 */

/**
 * States and helpers for the Metrics Tree.
 */
class MetricsTreeModel {
  constructor() {
    /** @public {?MetricsTreeNode} */
    this.rootNode = null;
  }

  /**
   * Creates MetricsTreeNode using commonly used fields.
   * @param {string} name
   * @param {?Array<!MetricsTreeNode>} children
   * @param {string} iconKey
   * @return {!MetricsTreeNode}
   * @public
   */
  makeDataNode(name, children, iconKey) {
    const node = /** @type {!MetricsTreeNode} */ ({name});
    if (children)
      node.children = children;
    if (iconKey)
      node.iconKey = iconKey;
    return node;
  };

  /**
   * Creates a MetricItems list summed from |items| from a list of leaf nodes.
   * @param {!Array<!MetricsTreeNode>} srcNodes
   * @return {!Array<!MetricsItem>}
   * @public
   */
  makeTotalItemList(srcNodes) {
    const diffMode = state.getDiffMode();
    const dstItems = /** @type {!Map<string, !MetricsItem>}*/ (new Map());
    const ret = /** @type {!Array<!MetricsItem>} */ ([]);

    for (const srcNode of srcNodes) {
      for (const srcItem of srcNode.items) {
        const metricName = srcItem.name;
        let dstItem = dstItems.get(metricName);
        if (!dstItem) {
          dstItem = /** @type {!MetricsItem} */ ({name: metricName, value: 0});
          if (diffMode) {
            dstItem.beforeValue = 0;
          }
          dstItems.set(metricName, dstItem);
        }
        dstItem.value += srcItem.value;
        if (diffMode)
          dstItem.beforeValue += srcItem.beforeValue;
      }
    }
    return Array.from(dstItems.values())
        .sort((a, b) => a.name.localeCompare(b.name));
  }

  /**
   * Extracts Metrics Tree data, and stores the result into |rootNode|.
   * @param {Object} metadata
   * @public
   */
  extractAndStoreRoot(metadata) {
    const EMPTY_OBJ = {};
    const diffMode = state.getDiffMode();

    const getOrMakeContainers = (size_file) => {
      if (size_file) {
        if (size_file.containers)
          return size_file.containers;
        // For old format without explicit containers, synthesize one.
        const container = {name: '(Default container)'};
        if (size_file.metrics_by_file)
          container.metrics_by_file = size_file.metrics_by_file;
        return [container];
      }
      return [];
    };
    const containers = getOrMakeContainers(metadata?.size_file);
    const beforeContainers = getOrMakeContainers(metadata?.before_size_file);

    const makeContainerMap = (curContainers) => {
      const ret = new Map();
      for (const c of curContainers) {
        ret.set(c.name, c);
      }
      return ret;
    };
    const containerMap = makeContainerMap(containers);
    const beforeContainerMap = makeContainerMap(beforeContainers);

    const containerNames = uniquifyIterToString(
        joinIter(containerMap.keys(), beforeContainerMap.keys()));
    const dexNodes = [];

    const rootNode = this.makeDataNode('Metrics', [], 'metrics');

    // Populate with containers -> files -> table.
    for (const containerName of containerNames) {
      const metricsByFile =
          containerMap.get(containerName)?.metrics_by_file ?? EMPTY_OBJ;
      const beforeMetricsByFile =
          beforeContainerMap.get(containerName)?.metrics_by_file ?? EMPTY_OBJ;
      const filenames = uniquifyIterToString(joinIter(
          Object.keys(metricsByFile), Object.keys(beforeMetricsByFile)));
      if (filenames.length === 0)
        continue;

      const containerNode = this.makeDataNode(containerName, [], 'group');
      containerNode.isFiltered = true;

      for (const filename of filenames) {
        const isDex = filename.endsWith('.dex');
        const metrics = metricsByFile[filename] ?? EMPTY_OBJ;
        const beforeMetrics = beforeMetricsByFile[filename] ?? EMPTY_OBJ;
        const metricNames = uniquifyIterToString(
            joinIter(Object.keys(metrics), Object.keys(beforeMetrics)));
        // |fileNode| has single |tableNode| child to enable UI show / hide.
        const fileNode = this.makeDataNode(filename, [], 'file');
        const tableNode = this.makeDataNode('', null, null);  // Leaf.
        tableNode.items = [];
        if (isDex)
          dexNodes.push(tableNode);
        for (const metricName of metricNames) {
          const item = /** @type {!MetricsItem} */ ({});
          item.name = metricName;
          item.value = metrics[metricName] ?? 0;
          if (diffMode)
            item.beforeValue = beforeMetrics[metricName] ?? 0;
          tableNode.items.push(item);
        }
        fileNode.children.push(tableNode);
        containerNode.children.push(fileNode);
      }
      rootNode.children.push(containerNode);
    }

    // Add special nodes to compute totals.
    if (dexNodes.length > 0) {
      const dexFileNode = this.makeDataNode('(DEX)', dexNodes, 'file');
      rootNode.totals = [dexFileNode];
    }
    this.rootNode = rootNode;
  }

  /**
   * Decides whether a MetricsTreeNode is a leaf node.
   * @param {!MetricsTreeNode} dataNode
   * @return {Boolean}
   * @public
   */
  isLeaf(dataNode) {
    return Boolean(dataNode.items);
  }

  /**
   * Visits all descendant leaf nodes from a list of nodes.
   * @param {!Array<!MetricsTreeNode>} srcNodes
   * @public @generator
   */
  * visitAllLeaves(srcNodes) {
    function* makeIter(nodes) {
      for (const node of nodes) {
        yield node;
      }
    }
    const st = [makeIter(srcNodes)];
    while (st.length > 0) {
      const v = st[st.length - 1].next();
      if (v.done) {
        st.pop();
      } else {
        const curNode = v.value;
        if (this.isLeaf(curNode)) {
          yield curNode;
        } else {  // Is group.
          st.push(makeIter(curNode.children));
        }
      }
    }
  }
}

/**
 * Class to manage UI to display a few tree levels (for containers and files),
 * with each leaf node displaying a table of metrics data.
 * @extends {TreeUi<MetricsTreeNode>}
 */
class MetricsTreeUi extends TreeUi {
  /** @param {!MetricsTreeModel} model */
  constructor(model) {
    super(g_el.ulMetricsTree);

    /** @private @const {!MetricsTreeModel} */
    this.model = model;

    /** @private {?function(string): boolean} */
    this.nameFilter = null;

    /** @private @const {function(!KeyboardEvent): *} */
    this.boundHandleKeyDown = this.handleKeyDown.bind(this);
  }

  /** @public */
  updateFilter() {
    this.nameFilter = state.getFilter();
  }

  /**
   * @param {!MetricsTreeNode} nodeData
   * @param {!HTMLTableElement} table
   * @private
   */
  populateMetricsTable(nodeData, table) {
    const diffMode = state.getDiffMode();
    if (nodeData.items.length > 0) {
      const headings = ['Name'];
      headings.push(...(diffMode ? ['Before', 'After', 'Diff'] : ['Value']))
      const tr = document.createElement('tr');
      for (const t of headings) {
        tr.appendChild(dom.textElement('th', t, ''));
      }
      table.appendChild(tr);
    }
    for (const item of nodeData.items) {
      const tr = document.createElement('tr');
      tr.appendChild(dom.textElement('td', item.name, ''));
      if (diffMode) {
        tr.appendChild(
            dom.textElement('td', formatNumber(item.beforeValue), 'number'));
      }
      tr.appendChild(dom.textElement('td', formatNumber(item.value), 'number'));
      if (diffMode) {
        const delta = item.value - item.beforeValue;
        if (delta !== 0)
          tr.classList.add(delta < 0 ? 'negative' : 'positive');
        tr.appendChild(
            dom.textElement('td', formatNumber(delta), 'number diff'));
      }
      table.appendChild(tr);
    }
  }

  /** @override @protected */
  makeGroupOrLeafFragment(nodeData) {
    const isLeaf = this.model.isLeaf(nodeData);
    // Use different template depending on whether node is group or leaf.
    const tmpl = isLeaf ? g_el.tmplMetricsTreeLeaf : g_el.tmplMetricsTreeGroup;
    const fragment = document.importNode(tmpl.content, true);
    const listItemElt = fragment.firstElementChild;
    const nodeElt =
        /** @type {HTMLAnchorElement} */ (listItemElt.firstElementChild);

    // Set the symbol name and hover text.
    if (nodeData.name) {
      const spanSymbolName = /** @type {HTMLSpanElement} */ (
          fragment.querySelector('.symbol-name'));
      spanSymbolName.textContent = nodeData.name;
      spanSymbolName.title = nodeData.name;
    }

    if (isLeaf)
      this.populateMetricsTable(nodeData, fragment.querySelector('table'));
    if (nodeData.iconKey) {
      // Insert type dependent SVG icon at the start of |nodeElt|.
      const icon = getMetricsIconTemplate(nodeData.iconKey);
      nodeElt.insertBefore(icon, nodeElt.firstElementChild);
    }
    return {fragment, isLeaf};
  }

  /** @override @protected */
  async getGroupChildrenData(link) {
    const data = this.uiNodeToData.get(link);
    const ret = data.children.filter(
        ch => !ch.isFiltered || !this.nameFilter || this.nameFilter(ch.name));

    // Dynamiclaly synthesize "(TOTAL)" node data.
    if (data.totals) {
      // Create set of table leaf nodes, for filtering.
      const leafSet = new Set(this.model.visitAllLeaves(ret));
      // Create "(TOTAL)" node data lazily, i.e., skip if empty.
      let totalNode = null;
      for (const totalData of data.totals) {
        const srcNodes = totalData.children.filter(ch => leafSet.has(ch));
        if (srcNodes.length === 0)
          continue;
        const title = totalData.name + `: ${srcNodes.length}`;
        const tableNode = this.model.makeDataNode('', null, null);  // Leaf.
        tableNode.items = this.model.makeTotalItemList(srcNodes);
        const fileNode = this.model.makeDataNode(title, [tableNode], 'file');
        if (!totalNode) {
          totalNode = this.model.makeDataNode('(TOTAL)', [], 'metrics');
          ret.push(totalNode);
        }
        totalNode.children.push(fileNode);
      }
    }
    return ret;
  }

  /**
   * @param {!KeyboardEvent} event
   * @protected
   */
  handleKeyDown(event) {
    if (event.altKey || event.ctrlKey || event.metaKey)
      return;

    /** @type {!TreeNodeElement} */
    const nodeElt = /** @type {!TreeNodeElement} */ (event.target);
    /** @type {number} Index of this element in the node list */
    const focusIndex = Array.prototype.indexOf.call(this.liveNodeList, nodeElt);

    this.handleKeyNavigationCommon(event, nodeElt, focusIndex);
  }

  /** @override @public */
  init() {
    super.init();

    g_el.ulMetricsTree.addEventListener('keydown', this.boundHandleKeyDown);
  }
}
