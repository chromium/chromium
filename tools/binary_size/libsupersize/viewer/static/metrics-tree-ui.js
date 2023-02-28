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
 * @property {?MetricsTreeNode} parent - Parent tree node, null if this is a
 *     root node.
 * @property {!Array<!MetricsTreeNode>|undefined} children - Child nodes.
 *     Non-existent or null indicates this is a leaf node.
 * @property {!Array<!MetricsItem>|undefined} items - For leaf nodes only, a
 *     list of named values for a metric. Mutually exclusive with |liveItems|.
 * @property {?function(function(string): boolean): !Array<!MetricsItem>|
 *            undefined} liveItems - For leaf nodes only, function to return a
 *     list of named values for a metric, taking a filtering function. To be
 *     included, the filter is eithe rnull, or needs to return true for names of
 *     all ancestor with |isFiltered| true. Mutually exclusive with |item|.
 * @property {boolean|undefined} isFiltered - For group nodes only, whether
 *     filtering is applied to the node.
 * @property {string|undefined} iconKey - For group nodes only, input for
 *     getMetricsIconTemplate() to retrieve icon.
 */

/**
 * States and helpers for the Metrics Tree.
 */
class MetricsTreeModel {
  /**
   * Extracts the underlying tree data for Metrics Tree rendering, and returns
   * the root data node.
   * @param {Object} metadata
   * @return {!MetricsTreeNode}
   */
  static extract(metadata) {
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

    /**
     * @param {string} name
     * @param {?Array<!MetricsTreeNode>} children
     * @param {string} iconKey
     * @return !MetricsTreeNode
     */
    const makeNode = (name, children, iconKey) => {
      const node = /** @type {!MetricsTreeNode} */ ({name});
      if (children)
        node.children = children;
      if (iconKey)
        node.iconKey = iconKey;
      return node;
    };

    /**
     * @param {!MetricsTreeNode} parent
     * @param {!MetricsTreeNode} child
     */
    const addChild = (parent, child) => {
      parent.children.push(child);
      child.parent = parent;
    };

    const containerNames = uniquifyIterToString(
        joinIter(containerMap.keys(), beforeContainerMap.keys()));
    const dexNodes = [];

    const rootNode = makeNode('Metrics', [], 'metrics');
    rootNode.parent = null;

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

      const containerNode = makeNode(containerName, [], 'group');
      containerNode.isFiltered = true;

      for (const filename of filenames) {
        const isDex = filename.endsWith('.dex');
        const metrics = metricsByFile[filename] ?? EMPTY_OBJ;
        const beforeMetrics = beforeMetricsByFile[filename] ?? EMPTY_OBJ;
        const metricNames = uniquifyIterToString(
            joinIter(Object.keys(metrics), Object.keys(beforeMetrics)));
        // |fileNode| has single |tableNode| child to enable UI show / hide.
        const fileNode = makeNode(filename, [], 'file');
        const tableNode = makeNode('', null, null);  // Leaf.
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
        addChild(fileNode, tableNode);
        addChild(containerNode, fileNode);
      }
      addChild(rootNode, containerNode);
    }

    // Add special node to compute totals.
    if (dexNodes.length > 0) {
      const totalNode = makeNode('(TOTAL)', [], 'metrics');
      const dexFileNode = makeNode('(DEX)', [], 'file');
      const tableNode = makeNode('', null, null);  // Leaf.

      /**
       * @param {!Map<string, !MetricsItem>} items
       * @param {string} metricName
       * @return {!MetricsItem}
       */
      const getOrMakeMetricsItem = (items, metricName) => {
        let ret = items.get(metricName);
        if (!ret) {
          ret = /** @type {!MetricsItem} */ ({name: metricName, value: 0})
          if (diffMode) {
            ret.beforeValue = 0;
          }
          items.set(metricName, ret);
        }
        return ret;
      };

      /**
       * @param {!MetricsTreeNode} node
       * @param {function(string): boolean} nameFilter
       * @return {boolean}
       */
      const hasAncestorRejectedByFilter = (node, nameFilter) => {
        if (nameFilter) {
          for (; node; node = node.parent) {
            if (node.isFiltered && !nameFilter(node.name))
              return true;
          }
        }
        return false;
      };

      /** @param {function(string): boolean} nameFilter */
      tableNode.liveItems = (nameFilter) => {
        const dstItems = /** @type {!Map<string, !MetricsItem>}*/ (new Map());
        for (const srcNode of dexNodes) {
          // Skip if any ancestor with |isFiltered| fails |nameFilter|.
          if (hasAncestorRejectedByFilter(srcNode, nameFilter))
            continue;
          for (const srcItem of srcNode.items) {
            const dstItem = getOrMakeMetricsItem(dstItems, srcItem.name);
            dstItem.value += srcItem.value;
            if (diffMode)
              dstItem.beforeValue += srcItem.beforeValue;
          }
        }
        return Array.from(dstItems.values())
            .sort((a, b) => a.name.localeCompare(b.name));
      };

      addChild(dexFileNode, tableNode);
      addChild(totalNode, dexFileNode);
      addChild(rootNode, totalNode);
    }
    return rootNode;
  }
}

/**
 * Class to manage UI to display a few tree levels (for containers and files),
 * with each leaf node displaying a table of metrics data.
 * @extends {TreeUi<MetricsTreeNode>}
 */
class MetricsTreeUi extends TreeUi {
  constructor() {
    super(g_el.ulMetricsTree);

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
    const items = nodeData.items ?? nodeData.liveItems(this.nameFilter);
    if (items.length > 0) {
      const headings = ['Name'];
      headings.push(...(diffMode ? ['Before', 'After', 'Diff'] : ['Value']))
      const tr = document.createElement('tr');
      for (const t of headings) {
        tr.appendChild(dom.textElement('th', t, ''));
      }
      table.appendChild(tr);
    }
    for (const item of items) {
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
    const isLeaf = !nodeData.children;
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
    let data = this.uiNodeToData.get(link);
    return data.children.filter(
        ch => !ch.isFiltered || !this.nameFilter || this.nameFilter(ch.name));
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

  init() {
    super.init();

    g_el.ulMetricsTree.addEventListener('keydown', this.boundHandleKeyDown);
  }
}
