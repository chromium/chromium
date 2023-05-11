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
 * @typedef {Object} SizeWithUnit
 * @property {string} unit - Size unit, e.g., 'byte', or 'count'.
 * @property {number} value - Size or size delta.
 */

/**
 * @typedef {Object} MetricsTreeNode
 * @property {string|undefined} name - The full name of the node, and is shown
 *     in the UI.
 * @property {!Array<!MetricsTreeNode>|undefined} children - Child nodes.
 *     Non-existent or null indicates this is a leaf node.
 * @property {!Array<!MetricsItem>|undefined} items - For leaf nodes only, a
 *     list of named values for a metric.
 * @property {string|undefined} iconKey - For group nodes only, input for
 *     getMetricsIconTemplate() to retrieve icon.
 * @property {!SizeWithUnit|undefined} size - For group nodes only, size to
 *     be shown in span.size, and used for sorting. Typically this is the total
 *     of item sizes across leaves.
 * @property {boolean|undefined} sorted - For group nodes only, whether to sort
 *     children nodes in descending order in UI. This requires each child to be
 *     a group node with |size| defined, using a common |size.unit|.
 */

/**
 * States and helpers for the Metrics Tree.
 */
class MetricsTreeModel {
  constructor() {
    /**
     * Filter for containers and files returning whether "container/file" should
     * be displayed, with null = always show.
     * @public {?function(string): boolean}
     */
    this.containerFileFilter = null;

    /**
     * Cached metadata used to create |rootNode|.
     * @public {?Object}
     */
    this.metadata = null;

    /**
     * Root node of the metrics tree, can be regenerated on UI change.
     * @public {?MetricsTreeNode}
     */
    this.rootNode = null;
  }

  /** @public */
  updateFilter() {
    this.containerFileFilter = state.getFilter();
  }

  /**
   * Visits multiple containers and files therein, applies
   * |containerFileFilter|, visits each metric ([name] -> value), and returns a
   * nested map [metric name] -> ([path = container/file] ->  metric value).
   * @param {!Array<!Object>} containers
   * @return {!DefaultMap<string, !Map<string, number>>}
   * @private
   */
  transposeMetrics(containers) {
    const metricNameToData =
        /** @type {!DefaultMap<string, !Map<string, number>>} */ (
            new DefaultMap(
                (unused) => /** @type {!Map<string, number>}*/ (new Map())));
    for (const c of containers) {
      if (!c.metrics_by_file ||
          (this.containerFileFilter && !this.containerFileFilter(c.name))) {
        continue;
      }
      for (const [file, metrics] of Object.entries(c.metrics_by_file)) {
        for (const [metricName, metricValue] of Object.entries(metrics)) {
          const data = metricNameToData.forcedGet(metricName);
          data.set(c.name + '/' + file, metricValue);
        }
      }
    }
    return metricNameToData;
  }

  /**
   * Creates MetricsTreeNode populated with commonly used fields.
   * @param {string} name
   * @param {?Array<!MetricsTreeNode>} children
   * @param {string} iconKey
   * @return {!MetricsTreeNode}
   * @private
   */
  makeDataNode(name, children, iconKey) {
    const node = /** @type {!MetricsTreeNode} */ ({name});
    if (children)
      node.children = children;
    if (iconKey)
      node.iconKey = iconKey;
    return node;
  }

  /**
   * Specialized makeDataNode() for metrics.
   * @param {!Set<string>} extensionSet
   * @param {string} metricSuffix
   * @return {!MetricsTreeNode}
   * @private
   */
  makeMetricNode(extensionSet, metricSuffix) {
    // Use destructuring to distinguish size-{0, 1, 2+} cases.
    const [first, second] = extensionSet;
    let ext = 'other';                // Default value for size-2+ cases.
    if (second == null) {             // Size-{0, 1}.
      if (!first || first == 'so') {  // Size-0, or Size-1 for ELF.
        ext = 'elf'
      } else if (first === 'arsc' || first === 'dex') {  // Size-1, known.
        ext = first;
      }  // Else Size-1, unknown: Keep ext = 'other'.
    }
    const prefix = ext.toUpperCase();
    return this.makeDataNode(`${prefix}: ${metricSuffix}`, [], ext);
  }

  /**
   * Extracts Metrics Tree data, and stores the result into |rootNode|.
   * @param {?Object} metadata Source metadata, must be non-null on first call.
   *     Subsequently, null means to use cached copy from previous call.
   * @public
   */
  extractAndStoreRoot(metadata) {
    const EMPTY_OBJ = {};
    const EMPTY_MAP = new Map();
    const diffMode = state.getDiffMode();
    if (metadata)
      this.metadata = metadata;

    const containers = getOrMakeContainers(this.metadata?.size_file);
    const beforeContainers =
        getOrMakeContainers(this.metadata?.before_size_file);

    /** @type {!Map<string, !Map<string, number>>} */
    const metricNameToData = this.transposeMetrics(containers);
    /** @type {!Map<string, !Map<string, number>>} */
    const beforeMetricNameToData = this.transposeMetrics(beforeContainers);
    const metricNames = uniquifyIterToString(
        joinIter(metricNameToData.keys(), beforeMetricNameToData.keys()));

    const rootNode = this.makeDataNode('Metrics', [], 'metrics');

    const lazyPrefixNodeMap =
        /** @type {!DefaultMap<string, !MetricsTreeNode>} */ (
            new DefaultMap((prefix) => {
              const name = prefix.slice(0, 1) + prefix.slice(1).toLowerCase()
              const prefixNode = this.makeDataNode(name, [], 'metrics');
              prefixNode.sorted = true;
              rootNode.children.push(prefixNode);
              return prefixNode;
            }));

    for (const metricName of metricNames) {
      // E.g., |metricName| = 'SIZE/.text',
      const data = metricNameToData.get(metricName) ?? EMPTY_MAP;
      const beforeData = beforeMetricNameToData.get(metricName) ?? EMPTY_MAP;
      const paths =
          uniquifyIterToString(joinIter(data.keys(), beforeData.keys()));

      const tableNode = this.makeDataNode('', null, null);  // Leaf.
      tableNode.items = [];

      const totalItem = /** @type {!MetricsItem} */ ({});
      totalItem.name = '(TOTAL)';
      totalItem.value = 0;
      if (diffMode)
        totalItem.beforeValue = 0;
      const extensionSet = new Set();
      for (const path of paths) {
        const ext = getFileExtension(path);
        if (ext)
          extensionSet.add(ext);
        const item = /** @type {!MetricsItem} */ ({});
        item.name = path;
        item.value = data.get(path) ?? 0;
        totalItem.value += item.value;
        if (diffMode) {
          item.beforeValue = beforeData.get(path) ?? 0;
          totalItem.beforeValue += item.beforeValue;
        }
        tableNode.items.push(item);
      }
      tableNode.items.push(totalItem);

      // E.g., 'SIZE/.text' => |prefix| = 'SIZE', |suffix| = '.text'.
      const prefix = metricName.split('/', 1)[0];
      const suffix = metricName.slice(prefix.length + 1);
      const metricNode = this.makeMetricNode(extensionSet, suffix);
      metricNode.children.push(tableNode);
      metricNode.size = /** @type{!SizeWithUnit} */ ({});
      // Heuristically get unit from |prefix|.
      metricNode.size.unit = (prefix === 'COUNT') ? 'count' : 'byte';
      metricNode.size.value = totalItem.value;
      if (diffMode)
        metricNode.size.value -= totalItem.beforeValue;
      const prefixNode = lazyPrefixNodeMap.forcedGet(prefix);
      prefixNode.children.push(metricNode);
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

    /** @private @const {function(!KeyboardEvent): *} */
    this.boundHandleKeyDown = this.handleKeyDown.bind(this);
  }

  /**
   * Replaces the contents of the size element for a group node.
   * @param {!SizeWithUnit} size Data to be shown.
   * @param {HTMLElement} sizeElt Element to display size.
   * @private
   */
  setSize(size, sizeElt) {
    let newSizeElt = null;
    if (size.unit === 'byte') {
      newSizeElt = makeBytesElement(size.value);
      setSizeClasses(sizeElt, size.value, false);
    } else {
      // newSizeElt = makeBytesElement(size.value);
      newSizeElt = document.createTextNode(formatNumber(size.value));
      setSizeClasses(sizeElt, size.value, true);
    }
    dom.replace(sizeElt, newSizeElt);
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

    if (!isLeaf && nodeData.size)
      this.setSize(nodeData.size, nodeElt.querySelector('.size'));

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
    const ret = Array.from(data.children);
    if (data.sorted) {
      // If specified, sort by descending absolute value, then by name.
      ret.sort((a, b) => {
        const d = Math.abs(b.size.value) - Math.abs(a.size.value);
        return d !== 0 ? d : a.name.localeCompare(b.name);
      });
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

    // When the "byteunit" state changes, update all .size elements.
    state.stByteUnit.addObserver(() => {
      for (const link of this.liveNodeList) {
        const size = this.uiNodeToData.get(link).size;
        if (size)
          this.setSize(size, link.querySelector('.size'));
      }
    });

    g_el.ulMetricsTree.addEventListener('keydown', this.boundHandleKeyDown);
  }
}
