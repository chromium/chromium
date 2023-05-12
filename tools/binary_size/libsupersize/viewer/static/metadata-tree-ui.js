// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and helpers for viewing metadata as tree and/or table.
 */

/**
 * @typedef {Object} MetadataItem
 * @property {string} name - Item name.
 * @property {number|string} value - Metadata value.
 * @property {number|string|undefined} beforeValue - Optional "before" metadata
 *   value.
 */

/**
 * @typedef {Object} MetadataTreeNode
 * @property {string|undefined} name - The full name of the node, and is shown
 *     in the UI.
 * @property {!Array<!MetadataTreeNode>|undefined} children - Child nodes.
 *     Non-existent or null indicates this is a leaf node.
 * @property {!Array<!MetadataItem>|undefined} items - For leaf node only, a
 *     list of named metadata values.
 * @property {string|undefined} iconKey - Override value for
 *     getMetadataIconTemplate() to retrieve icon.
 */

/**
 * States and helpers for the Metadata Tree.
 */
class MetadataTreeModel {
  constructor() {
    /**
     * Cached metadata used to create |rootNode|.
     * @public {?Object}
     */
    this.metadata = null;

    /**
     * Root node of the metadata tree, can be regenerated on UI change.
     * @public {?MetadataTreeNode}
     */
    this.rootNode = null;
  }

  /**
   * Renders primitive |value| to string, and an array of primitive values to
   * primitive values separated by '\n'.
   * @param {?Object} value
   * @return {string}
   * @private
   */
  renderSimpleValue(value) {
    if (value === null)
      return 'null';
    const t = typeof value;
    if (t === 'number')
      return formatNumber(value);
    if (t !== 'object')
      return value.toString();
    if (Array.isArray(value)) {
      if (value.every((v) => v === null || typeof v !== 'object'))
        return value.join('\n');
    }
    return '[Object]';
  }

  /**
   * Creates MetadataTreeNode populated with commonly used fields.
   * @param {string} name
   * @param {?Array<!MetadataTreeNode>} children
   * @return {!MetadataTreeNode}
   * @private
   */
  makeDataNode(name, children) {
    const node = /** @type {!MetadataTreeNode} */ ({name});
    if (children)
      node.children = children;
    return node;
  }

  /**
   * Jointly visits two key-value objects to yield MetadataItems, with values
   * rendered to strings.
   * @param {boolean} diffMode
   * @param {!Object} obj
   * @param {!Object} beforeObj
   * @private @generator
   */
  * makeItems(diffMode, obj, beforeObj) {
    const keys = uniquifyIterToString(
        joinIter(Object.keys(obj), Object.keys(beforeObj)));
    for (const key of keys) {
      const item = /** @type {!MetadataItem} */ ({name: key});
      if (diffMode)
        item.beforeValue = this.renderSimpleValue(beforeObj[key] ?? '');
      item.value = this.renderSimpleValue(obj[key] ?? '');
      yield item;
    }
  }

  /**
   * Converts a container array to a Map from a distinct name to each container.
   * @param {!Array<!Object>} containers
   * @return {!Map<!Object>}
   * @private
   */
  containersToMap(containers) {
    const ret = new Map();
    for (const c of containers) {
      let name = c.name;
      while (ret.has(name))  // Ensures distinct name.
        name += '!';
      ret.set(name, c);
    }
    return ret;
  }

  /**
   * Jointly visits two container arrays to yield names and container pairs with
   * null placeholders.
   * @param {!Array<!Object>} containers
   * @param {!Array<!Object>} beforeContainers
   * @private @generator
   */
  * visitContainers(containers, beforeContainers) {
    const containerMap = this.containersToMap(containers);
    const beforeContainerMap = this.containersToMap(beforeContainers);
    const names = uniquifyIterToString(
        joinIter(containerMap.keys(), beforeContainerMap.keys()));
    for (const name of names) {
      yield {
        name,
        container: containerMap.get(name) ?? null,
        beforeContainer: beforeContainerMap.get(name) ?? null
      };
    }
  }

  /**
   * Converts arbitrary |metadata| to a consistent tree form, and stores the
   * result into |rootNode|.
   * @param {?Object} metadata Source metadata, must be non-null on first call.
   *     Subsequently, null means to use cached copy from previous call.
   * @public
   */
  extractAndStoreRoot(metadata) {
    const EMPTY_OBJ = {};
    const diffMode = state.getDiffMode();
    if (metadata)
      this.metadata = metadata;

    const containers = getOrMakeContainers(this.metadata.size_file);
    const beforeContainers =
        getOrMakeContainers(this.metadata.before_size_file);

    const rootNode = this.makeDataNode('Metadata', []);
    rootNode.iconKey = 'root';

    if (this.metadata.size_file?.build_config) {
      const configNode = this.makeDataNode('', null);
      const buildConfig = this.metadata.size_file?.build_config ?? EMPTY_OBJ;
      const beforeBuildConfig =
          this.metadata.before_size_file?.build_config ?? EMPTY_OBJ;
      configNode.items =
          [...this.makeItems(diffMode, buildConfig, beforeBuildConfig)];
      rootNode.children.push(configNode);
    }

    for (const {name, container, beforeContainer} of this.visitContainers(
             containers, beforeContainers)) {
      const subMetadata = container?.metadata ?? EMPTY_OBJ;
      const beforeSubMetadata = beforeContainer?.metadata ?? EMPTY_OBJ;
      const tableNode = this.makeDataNode('', null);
      tableNode.items =
          [...this.makeItems(diffMode, subMetadata, beforeSubMetadata)];
      const outerNode = this.makeDataNode(name, [tableNode]);
      rootNode.children.push(outerNode);
    }

    this.rootNode = rootNode;
  }

  /**
   * Decides whether a MetadataTreeNode is a leaf node.
   * @param {!MetadataTreeNode} dataNode
   * @return {Boolean}
   * @public
   */
  isLeaf(dataNode) {
    return !dataNode.children;
  }
}

/**
 * Class to manage UI to display metadata as a trees with tables as leaves.
 * @extends {TreeUi<MetadataTreeNode>}
 */
class MetadataTreeUi extends TreeUi {
  /** @param {!MetadataTreeModel} model */
  constructor(model) {
    super(g_el.ulMetadataTree);

    /** @private @const {!MetadataTreeModel} */
    this.model = model;

    /** @private @const {function(!KeyboardEvent): *} */
    this.boundHandleKeyDown = this.handleKeyDown.bind(this);
  }

  /**
   * @param {!Array<!MetadataItem>} items
   * @param {!DocumentFragment} fragment
   * @private
   */
  populateTable(items, fragment) {
    const table = fragment.querySelector('table');
    const diffMode = state.getDiffMode();
    for (const item of items) {
      const tr = document.createElement('tr');
      tr.appendChild(dom.textElement('td', item.name, ''));
      const s2 = item.value.toString();
      const td = dom.textElement('td', s2, '');
      if (diffMode) {
        const s1 = item.beforeValue.toString();
        tr.appendChild(dom.textElement('td', s1, ''));
        td.classList.toggle('metadata-changed', s1 !== s2);
      }
      tr.appendChild(td);
      table.appendChild(tr);
    }
  }

  /** @override @protected */
  makeGroupOrLeafFragment(nodeData) {
    const isLeaf = this.model.isLeaf(nodeData);
    // Use different template depending on whether node is group or leaf.
    const tmpl =
        isLeaf ? g_el.tmplMetadataTreeLeaf : g_el.tmplMetadataTreeGroup;
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

    if (nodeData.items)
      this.populateTable(nodeData.items, fragment);

    // Insert type dependent SVG icon at the start of |nodeElt|.
    if (!isLeaf) {
      const icon = getMetadataIconTemplate(nodeData.iconKey ?? 'group');
      nodeElt.insertBefore(icon, nodeElt.firstElementChild);
    }
    return {fragment, isLeaf};
  }

  /** @override @protected */
  async getGroupChildrenData(link) {
    const data = this.uiNodeToData.get(link);
    return data.children;
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

    g_el.ulMetadataTree.addEventListener('keydown', this.boundHandleKeyDown);
  }
}
