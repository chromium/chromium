// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and helpers for viewing and interacting with the Symbol Tree.
 */

/**
 * Class to manage UI to display a hierarchical TreeNode, supporting branch
 * expansion and contraction with dynamic loading.
 * @extends {TreeUi<TreeNode>}
 */
class SymbolTreeUi extends TreeUi {
  constructor() {
    super(g_el.ulSymbolTree);

    /**
     * @protected @const {RegExp} Capture one of: "::", "../", "./", "/", "#".
     */
    this.SPECIAL_CHAR_REGEX = /(::|(?:\.*\/)+|#)/g;

    /**
     * @protected @const {string} Insert zero-width space after capture group.
     */
    this.ZERO_WIDTH_SPACE = '$&\u200b';

    /**
     * Expansion cascade plan: If X -> Y, then on expanding X, also expand
     * Y if Y -> Z exists, else set focus to Y.
     * @private @const {!Map<number, number>}
     */
    this.expansionIdMap = new Map();

    // Event listeners need to be bound to this, but each fresh .bind creates a
    // function, which wastes memory and not usable for removeEventListener().
    // The |_bound*()| functions aim to solve the abolve.

    /** @private @const {function(!KeyboardEvent): *} */
    this.boundHandleKeyDown = this.handleKeyDown.bind(this);

    /** @private @const {function(!MouseEvent): *} */
    this.boundHandleMouseOver = this.handleMouseOver.bind(this);

    /** @private @const {function(!MouseEvent): *} */
    this.boundHandleRefocus = this.handleRefocus.bind(this);

    /** @private @const {function(!MouseEvent): *} */
    this.boundHandleFocusIn = this.handleFocusIn.bind(this);

    /** @private @const {function(!MouseEvent): *} */
    this.boundHandleFocusOut = this.handleFocusOut.bind(this);
  }

  /**
   * Displays an error modal to indicate that the symbol tree is empty.
   * @param {boolean} show
   */
  toggleNoSymbolsMessage(show) {
    g_el.divNoSymbolsMsg.style.display = show ? '' : 'none';
  }

  /**
   * Replaces the contents of the size element for a tree node.
   * @param {!TreeNode} nodeData Data about this size element's tree node.
   * @param {HTMLElement} sizeElt Element to display size.
   * @private
   */
  setSize(nodeData, sizeElt) {
    const {description, element, value} = getSizeContents(nodeData);

    // Replace the contents of '.size' and change its title
    dom.replace(sizeElt, element);
    sizeElt.title = description;
    setSizeClasses(sizeElt, value, state.stMethodCount.get());
  }

  /** @override @protected */
  makeGroupOrLeafFragment(nodeData) {
    const isLeaf = nodeData.children && nodeData.children.length === 0;
    // Use different template depending on whether node is group or leaf.
    const tmpl = isLeaf ? g_el.tmplSymbolTreeLeaf : g_el.tmplSymbolTreeGroup;
    const fragment = document.importNode(tmpl.content, true);
    const listItemElt = fragment.firstElementChild;
    const nodeElt =
        /** @type {HTMLAnchorElement} */ (listItemElt.firstElementChild);

    // Insert type dependent SVG icon at the start of |nodeElt|.
    const fill = isLeaf ? null : getIconStyle(nodeData.type[1]).color;
    const icon = getIconTemplateWithFill(nodeData.type[0], fill);
    nodeElt.insertBefore(icon, nodeElt.firstElementChild);

    // Insert diff status dependent SVG icon at the start of |listItemElt|.
    const diffStatusIcon = getDiffStatusTemplate(nodeData);
    if (diffStatusIcon)
      listItemElt.insertBefore(diffStatusIcon, listItemElt.firstElementChild);

    // Set the symbol name and hover text.
    /** @type {HTMLSpanElement} */
    const symbolName = fragment.querySelector('.symbol-name');
    symbolName.textContent = shortName(nodeData).replace(
        this.SPECIAL_CHAR_REGEX, this.ZERO_WIDTH_SPACE);
    symbolName.title = nodeData.idPath;

    // Set the byte size and hover text.
    this.setSize(nodeData, fragment.querySelector('.size'));

    nodeElt.addEventListener('mouseover', this.boundHandleMouseOver);
    return {fragment, isLeaf};
  }

  /** @override @protected */
  async getGroupChildrenData(link) {
    // If the children data have not yet been loaded, request from the worker.
    let data = this.uiNodeToData.get(link);
    if (!data?.children) {
      /** @type {HTMLSpanElement} */
      const symbolName = link.querySelector('.symbol-name');
      const idPath = symbolName.title;
      data = await window.supersize.worker.openNode(idPath);
      this.uiNodeToData.set(link, data);
    }
    return data.children;
  }

  /** @override */
  autoExpandAttentionWorthyChild(link, childrenElements) {
    let nodeId = this.uiNodeToData.get(link).id;
    if (this.expansionIdMap.has(nodeId)) {
      const nextChildId = this.expansionIdMap.get(nodeId);
      // Consume expansion link |nodeId| -> |nextChildId| to avoid interfering
      // with regular UI.
      this.expansionIdMap.delete(nodeId);
      if (nextChildId != null) {
        for (const childElement of childrenElements) {
          const childNode = childElement.querySelector('.node');
          const childId = this.uiNodeToData.get(childNode).id;
          if (childId === nextChildId) {
            if (this.expansionIdMap.has(childId)) {
              // Found the child to expand: Click to expand and propagate.
              childNode.click();
              return;
            }
            // |nextChildId|'s absence in |expansionIdMap| means it's the target
            // node ID, so set focus. Use dom.onNodeAdded() since |childElement|
            // (and hence |childNode|) might not be added to the DOM yet.
            dom.onNodeAdded(childNode, () => childNode.focus());
            // Continue to default behavior, which may cause more expansion.
            break;
          }
        }
      }
    }
    super.autoExpandAttentionWorthyChild(link, childrenElements);
  }

  /**
   * Adds a path to |expansionIdMap| to cause expansion cascade.
   * @param {!Array<number>} nodePathIds
   * @public
   */
  planPathExpansion(nodePathIds) {
    for (let i = 1; i < nodePathIds.length; ++i) {
      this.expansionIdMap.set(nodePathIds[i - 1], nodePathIds[i]);
    }
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

    if (this.handleKeyNavigationCommon(event, nodeElt, focusIndex))
      return;

    /**
     * Focuses the tree element at |index| if it starts with |ch|.
     * @param {string} ch
     * @param {number} index
     * @return {boolean} True if the short name did start with |ch|.
     */
    const focusIfStartsWith = (ch, index) => {
      const data = this.uiNodeToData.get(this.liveNodeList[index]);
      if (shortName(data).startsWith(ch)) {
        event.preventDefault();
        this.setFocusElementByIndex(index);
        return true;
      }
      return false;
    };

    // If a letter was pressed, find a node starting with that character.
    if (event.key.length === 1 && event.key.match(/\S/)) {
      // Check all nodes below this one.
      for (let i = focusIndex + 1; i < this.liveNodeList.length; i++) {
        if (focusIfStartsWith(event.key, i))
          return;
      }
      // Wrap around: Starting from the top, check all nodes above this one.
      for (let i = 0; i < focusIndex; i++) {
        if (focusIfStartsWith(event.key, i))
          return;
      }
    }
  }

  /**
   * Displays the infocard when a node is hovered over, unless a node is
   * currently focused.
   * @param {!MouseEvent} event
   * @protected
   */
  handleMouseOver(event) {
    const active = document.activeElement;
    if (!active || !active.classList.contains('node')) {
      displayInfocard(this.uiNodeToData.get(
          /** @type {HTMLElement} */ (event.currentTarget)));
    }
  }

  /**
   * Mousedown handler for an already-focused leaf node, to toggle it off.
   * @param {!MouseEvent} event
   * @protected
   */
  handleRefocus(event) {
    // Prevent click that would cause another focus event.
    event.preventDefault();
    /** @type {!HTMLElement} */ (event.currentTarget).blur();
    // Let focusout handles the cleanup.
  }

  /**
   * Focusin handler for a node.
   * @param {!MouseEvent} event
   * @protected
   */
  handleFocusIn(event) {
    const elt = /** @type {!HTMLElement} */ (event.target);
    if (this.isTerminalElement(elt))
      elt.addEventListener('mousedown', this.boundHandleRefocus);
    const data = /** @type {!TreeNode} */ (this.uiNodeToData.get(elt));
    displayInfocard(data);
    /** @type {HTMLElement} */ (event.currentTarget)
        .parentElement.classList.add('focused');
    state.stFocus.set(data.id.toString());
  }

  /**
   * Focusout handler for a node.
   * @param {!MouseEvent} event
   * @protected
   */
  handleFocusOut(event) {
    const elt = /** @type {!HTMLElement} */ (event.target);
    if (this.isTerminalElement(elt))
      elt.removeEventListener('mousedown', this.boundHandleRefocus);
    /** @type {HTMLElement} */ (event.currentTarget)
        .parentElement.classList.remove('focused');
  }

  /** @override @protected */
  onTreeBlur() {
    state.stFocus.set('');
  }

  /** @override @public */
  init() {
    super.init();

    // When the "byteunit" state changes, update all .size elements.
    state.stByteUnit.addObserver(() => {
      for (const link of this.liveNodeList) {
        /** @type {HTMLElement} */
        const sizeElt = link.querySelector('.size');
        this.setSize(this.uiNodeToData.get(link), sizeElt);
      }
    });

    g_el.ulSymbolTree.addEventListener('keydown', this.boundHandleKeyDown);
    g_el.ulSymbolTree.addEventListener('focusin', this.boundHandleFocusIn);
    g_el.ulSymbolTree.addEventListener('focusout', this.boundHandleFocusOut);
  }
}
