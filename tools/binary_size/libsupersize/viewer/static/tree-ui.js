// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and methods for the Tree View in the
 * Binary Size Analysis HTML report.
 */

/**
 * @typedef {HTMLAnchorElement | HTMLSpanElement} TreeNodeElement
 */

/**
 * Base class for UI to render and navigate a tree structure. In DOM this is
 * rendered as a nested <ul> with <li> for each vertex. Each vertex can be:
 * * A "group" containing <a class="node"> for structure. A group can be
 *   expanded or unexpanded, and is controlled by the base class.
 * * A "leaf" containing <span class="node"> for data ldaves. A leaf is
 ,   controlled by derived classes.
 * Element rendering is done by derived classes, using custom templates.
 *
 * @template NODE_DATA_TYPE The data type of a tree node (groups and leaves)
 */
class TreeUi {
  /** @param {!HTMLUListElement} rootElt */
  constructor(rootElt) {
    /**
     * @protected {HTMLCollectionOf<!TreeNodeElement>} Collection of all tree
     * node elements. Updates itself automatically.
     */
    this.liveNodeList =
        /** @type {HTMLCollectionOf<!TreeNodeElement>} */ (
            rootElt.getElementsByClassName('node'));

    /**
     * @protected @const {!WeakMap<HTMLElement, Readonly<NODE_DATA_TYPE>>}
     * Maps from UI nodes to data object to enable queries by event listeners
     * and other methods.
     */
    this.uiNodeToData = new WeakMap();

    /** @private @const {function(!MouseEvent): *} */
    this.boundToggleGroupElement = this.toggleGroupElement.bind(this);
  }

  /**
   * Decides whether |elt| is the node of a leaf or an unexpanded group.
   * @param {!HTMLElement} elt
   * @return {boolean}
   * @protected
   */
  isTerminalElement(elt) {
    return elt.classList.contains('node') &&
        elt.getAttribute('aria-expanded') === null;
  }

  /**
   * Sets focus to a new tree element while updating the element that last had
   * focus. The tabindex property is used to avoid needing to tab through every
   * single tree item in the page to reach other areas.
   * @param {?TreeNodeElement} nodeElt A tree node element.
   * @protected
   */
  setFocusElement(nodeElt) {
    const lastFocused = /** @type {HTMLElement} */ (document.activeElement);
    // If the last focused element was a tree node element, change its tabindex.
    if (this.uiNodeToData.has(lastFocused))
      lastFocused.tabIndex = -1;
    if (nodeElt) {
      nodeElt.tabIndex = 0;
      nodeElt.focus();
    }
  }

  /**
   * Same as setFocusElement(), but takes index into |liveNodeList| instead.
   * @param {number} index
   * @protected
   */
  setFocusElementByIndex(index) {
    this.setFocusElement(this.liveNodeList[index]);
  }

  /**
   * Creates an element for |nodeData| to represent a group or a leaf, which
   * depends on whether there are >= 1 children. May bind events.
   * @param {!NODE_DATA_TYPE} nodeData
   * @return {!{fragment: !DocumentFragment, isLeaf: boolean}}
   * @abstract @protected
   */
  makeGroupOrLeafFragment(nodeData) {
    return null;
  }

  /**
   * Creates an Element for |nodeData|, and binds click on group nodes to
   * toggleGroupElement().
   * @param {!NODE_DATA_TYPE} nodeData
   * @return {!DocumentFragment}
   * @public
   */
  makeNodeElement(nodeData) {
    const {fragment, isLeaf} = this.makeGroupOrLeafFragment(nodeData);
    const nodeElt = /** @type {TreeNodeElement} */ (
        assertNotNull(fragment.querySelector('.node')));

    // Associate clickable node & tree data.
    this.uiNodeToData.set(nodeElt, Object.freeze(nodeData));

    // Add click-to-toggle to group nodes.
    if (!isLeaf)
      nodeElt.addEventListener('click', this.boundToggleGroupElement);

    return fragment;
  }

  /**
   * Gets data for children of a group. Note that |link| is passed instead
   * @param {!HTMLAnchorElement} link
   * @return {!Promise<!Array<!NODE_DATA_TYPE>>}
   * @abstract @protected
   */
  async getGroupChildrenData(link) {
    return null;
  }

  /**
   * Populates |link| with
   * @param {!HTMLAnchorElement} link
   * @protected
   */
  async expandGroupElement(link) {
    const childrenData = await this.getGroupChildrenData(link);
    const newElements = childrenData.map((data) => this.makeNodeElement(data));
    if (newElements.length === 1) {
      // Open inner element if it only has a single child; this ensures nodes
      // like "java"->"com"->"google" are opened all at once.
      /** @type {!TreeNodeElement} */
      const childLink = newElements[0].querySelector('.node');
      childLink.click();  // Can trigger further expansion.
    }
    const newElementsFragment = dom.createFragment(newElements);
    requestAnimationFrame(() => {
      link.nextElementSibling.appendChild(newElementsFragment);
    });
  }

  /**
   * Click event handler to expand or close a group node.
   * @param {Event} event
   * @protected
   */
  async toggleGroupElement(event) {
    event.preventDefault();

    // Canonical structure:
    // <li>                       <!-- |treeitem| -->
    //   <a class="node">...</a>  <!-- |link| -->
    //   <ul>...</ul>             <!-- |group| -->
    // </li>
    const link = /** @type {!HTMLAnchorElement} */ (event.currentTarget);
    const treeitem = /** @type {!HTMLLIElement} */ (link.parentElement);
    const group = /** @type {!HTMLUListElement} */ (link.nextElementSibling);

    const isExpanded = treeitem.getAttribute('aria-expanded') === 'true';
    if (isExpanded) {
      // Take keyboard focus from descendent node.
      const lastFocused = /** @type {HTMLElement} */ (document.activeElement);
      if (lastFocused && group.contains(lastFocused))
        this.setFocusElement(link);
      // Update DOM.
      treeitem.setAttribute('aria-expanded', 'false');
      dom.replace(group, null);
    } else {
      treeitem.setAttribute('aria-expanded', 'true');
      await this.expandGroupElement(link);
    }
  }

  /**
   * Helper to handle tree navigation on keydown event.
   * @param {!KeyboardEvent} event Event passed from keydown event listener.
   * @param {!TreeNodeElement} link Tree node element, either a group or leaf.
   *     Trees use <a> tags, leaves use <span> tags. For example, see
   *     #tmpl-symbol-tree-group and #tmpl-symbol-tree-leaf.
   * @param {number} focusIndex
   * @return {boolean} Whether the event is handled.
   * @protected
   */
  handleKeyNavigationCommon(event, link, focusIndex) {
    /** Focuses the tree element immediately following this one. */
    const focusNext = () => {
      if (focusIndex > -1 && focusIndex < this.liveNodeList.length - 1) {
        event.preventDefault();
        this.setFocusElementByIndex(focusIndex + 1);
      }
    };

    /** Opens or closes the tree element. */
    const toggle = () => {
      event.preventDefault();
      /** @type {HTMLAnchorElement} */ (link).click();
    };

    switch (event.key) {
      // Space should act like clicking or pressing enter & toggle the tree.
      case ' ':
        toggle();
        return true;
      // Move to previous focusable node.
      case 'ArrowUp':
        if (focusIndex > 0) {
          event.preventDefault();
          this.setFocusElementByIndex(focusIndex - 1);
        }
        return true;
      // Move to next focusable node.
      case 'ArrowDown':
        focusNext();
        return true;
      // If closed tree, open tree. Otherwise, move to first child.
      case 'ArrowRight': {
        const expanded = link.parentElement.getAttribute('aria-expanded');
        // Handle groups only (leaves do not have aria-expanded property).
        if (expanded !== null) {
          if (expanded === 'true') {
            focusNext();
          } else {
            toggle();
          }
        }
        return true;
      }
      // If opened tree, close tree. Otherwise, move to parent.
      case 'ArrowLeft': {
        const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
        if (isExpanded) {
          toggle();
        } else {
          const groupList = link.parentElement.parentElement;
          if (groupList.getAttribute('role') === 'group') {
            event.preventDefault();
            /** @type {HTMLAnchorElement} */
            const parentLink = /** @type {HTMLAnchorElement} */ (
                groupList.previousElementSibling);
            this.setFocusElement(parentLink);
          }
        }
        return true;
      }
      // Focus first node.
      case 'Home':
        event.preventDefault();
        this.setFocusElementByIndex(0);
        return true;
      // Focus last node on screen.
      case 'End':
        event.preventDefault();
        this.setFocusElementByIndex(this.liveNodeList.length - 1);
        return true;
      // Expand all sibling nodes.
      case '*':
        const groupList = link.parentElement.parentElement;
        if (groupList.getAttribute('role') === 'group') {
          event.preventDefault();
          for (const li of groupList.children) {
            if (li.getAttribute('aria-expanded') !== 'true') {
              const otherLink =
                  /** @type {!TreeNodeElement} */ (li.querySelector('.node'));
              otherLink.click();
            }
          }
        }
        return true;
      // Remove focus from the tree view.
      case 'Escape':
        link.blur();
        return true;
    }

    return false;
  }

  /** @public */
  init() {}
}

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
   * Replaces the contents of the size element for a tree node.
   * @param {!TreeNode} nodeData Data about this size element's tree node.
   * @param {HTMLElement} sizeElement Element that should display the size
   * @private
   */
  setSize(nodeData, sizeElement) {
    const {description, element, value} = getSizeContents(nodeData);

    // Replace the contents of '.size' and change its title
    dom.replace(sizeElement, element);
    sizeElement.title = description;
    setSizeClasses(sizeElement, value);
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
    // focusout handler will handle cleanup.
    /** @type {!HTMLElement} */ (event.currentTarget).blur();
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
    displayInfocard(/** @type {!TreeNode} */ (this.uiNodeToData.get(elt)));
    /** @type {HTMLElement} */ (event.currentTarget)
        .parentElement.classList.add('focused');
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

  /** @public @override */
  init() {
    super.init();

    // When the "byteunit" state changes, update all .size elements.
    state.stByteUnit.addObserver(() => {
      for (const link of this.liveNodeList) {
        /** @type {HTMLElement} */
        const element = link.querySelector('.size');
        this.setSize(this.uiNodeToData.get(link), element);
      }
    });

    g_el.ulSymbolTree.addEventListener('keydown', this.boundHandleKeyDown);
    g_el.ulSymbolTree.addEventListener('focusin', this.boundHandleFocusIn);
    g_el.ulSymbolTree.addEventListener('focusout', this.boundHandleFocusOut);
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

{
  class ProgressBar {
    /** @param {!HTMLProgressElement} elt */
    constructor(elt) {
      /** @private {HTMLProgressElement} */
      this.elt = elt;

      /** @private {number} */
      this.prevVal = this.elt.value;
    }

    /** @param {number} val */
    setValue(val) {
      if (val === 0 || val >= this.prevVal) {
        this.elt.value = val;
        this.prevVal = val;
      } else {
        // Reset to 0 so the progress bar doesn't animate backwards.
        this.setValue(0);
        requestAnimationFrame(() => this.setValue(val));
      }
    }
  }

  /** @type {ProgressBar} */
  const _progress = new ProgressBar(g_el.progAppbar);

  /** @type {!SymbolTreeUi} */
  const _symbolTreeUi = new SymbolTreeUi();
  _symbolTreeUi.init();

  /** @type {?MetricsTreeNode} */
  let _metricsRootData = null;

  /** @type {!MetricsTreeUi} */
  const _metricsTreeUi = new MetricsTreeUi();
  _metricsTreeUi.init();

  /** @param {TreeProgress} message */
  function onProgressMessage(message) {
    const {error, percent} = message;
    _progress.setValue(percent);
    document.body.classList.toggle('error', Boolean(error));
  }

  /**
   * Processes response of an initial load / upload.
   * @param {BuildTreeResults} message
   */
  function processLoadTreeResponse(message) {
    const {diffMode} = message;
    const {beforeBlobUrl, loadBlobUrl, isMultiContainer, metadata} =
        message.loadResults;
    console.log(
        '%cPro Tip: %cawait supersize.worker.openNode("$FILE_PATH")',
        'font-weight:bold;color:red;', '')

    displayOrHideDownloadButton(beforeBlobUrl, loadBlobUrl);

    state.setDiffMode(diffMode);
    document.body.classList.toggle('diff', Boolean(diffMode));

    processBuildTreeResponse(message);
    renderAndShowMetricsTree(metadata);
    setMetadataContent(metadata);
    g_el.divMetadataView.classList.toggle('active', true);
    setReviewInfo(metadata);
  }

  /**
   * Sets the review URL and title from message to the HTML element.
   * @param {MetadataType} metadata
   */
  function setReviewInfo(metadata) {
    const processReviewInfo = (field) => {
      const urlExists = Boolean(
          field?.hasOwnProperty('url') && field?.hasOwnProperty('title'));
      if (urlExists) {
        g_el.linkReviewText.href = field['url'];
        g_el.linkReviewText.textContent = field['title'];
      }
      g_el.divReviewInfo.style.display = urlExists ? '' : 'none';
    };
    const sizeFile = metadata['size_file'];
    if (sizeFile?.hasOwnProperty('build_config')) {
      processReviewInfo(sizeFile['build_config'])
    }
  }

  /**
   * Processes the result of a buildTree() message.
   * @param {BuildTreeResults} message
   */
  function processBuildTreeResponse(message) {
    const {root} = message;
    _progress.setValue(1);

    const noSymbols = (Object.keys(root.childStats).length === 0);
    toggleNoSymbolsMessage(noSymbols);

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (root) {
      rootElement = _symbolTreeUi.makeNodeElement(root);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node
      link.click();
      link.tabIndex = 0;
    }

    // Double requestAnimationFrame ensures that the code inside executes in a
    // different frame than the above tree element creation.
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        dom.replace(g_el.ulSymbolTree, rootElement);
      });
    });
  }

  /**
   * Displays/hides download buttons for loadUrl.size and beforeUrl.size.
   * @param {?string=} beforeUrl
   * @param {?string=} loadUrl
   */
  function displayOrHideDownloadButton(beforeUrl = null, loadUrl = null) {
    const updateAnchor = (anchor, url) => {
      anchor.style.display = url ? '' : 'none';
      if (anchor.href && anchor.href.startsWith('blob:')) {
        URL.revokeObjectURL(anchor.href);
      }
      anchor.href = url;
    };
    updateAnchor(g_el.linkDownloadBefore, beforeUrl);
    updateAnchor(g_el.linkDownloadLoad, loadUrl);

    if (/** @type {string} */ (state.stLoadUrl.get()).includes('.sizediff')) {
      g_el.linkDownloadLoad.title = 'Download .sizediff file';
      g_el.linkDownloadLoad.download = 'load_size.sizediff';
    }
  }

  /**
   * Displays an error modal if the .sizediff file is empty.
   * @param {boolean} show
   */
  function toggleNoSymbolsMessage(show) {
    g_el.divNoSymbolsMsg.style.display = show ? '' : 'none';
  }

  /**
   * Extracts the underlying tree data for Metrics Tree rendering, and returns
   * the root data node.
   * @param {Object} metadata
   * @return {!MetricsTreeNode}
   */
  function extractMetricsTree(metadata) {
    const EMPTY_OBJ = {};

    const diffMode = state.getDiffMode();
    const containers = metadata?.size_file?.containers ?? [];
    const beforeContainers = metadata?.before_size_file?.containers ?? [];

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

  /**
   * Extracts metrics data and renders the Metrics Tree.
   * @param {?Object} metadata Used to compute Metrics Tree data. If null, reuse
   *     cached data. Otherwise new data is saved to cache.
   */
  function renderAndShowMetricsTree(metadata) {
    if (metadata)
      _metricsRootData = extractMetricsTree(metadata);
    _metricsTreeUi.updateFilter();

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (_metricsRootData) {
      rootElement = _metricsTreeUi.makeNodeElement(_metricsRootData);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Leave root UI node collapsed, but reachable by tab.
      link.tabIndex = 0;
    }

    dom.replace(g_el.ulMetricsTree, rootElement);
    g_el.divMetricsView.classList.toggle('active', true);
  }

  /**
   * Modifies metadata in-place so they render better.
   * @param {Object} metadata
   */
  function formatMetadataInPlace(metadata) {
    if (metadata?.hasOwnProperty('elf_mtime')) {
      const date = new Date(metadata['elf_mtime'] * 1000);
      metadata['elf_mtime'] = date.toString();
    }
  }

  /**
   * Renders the metadata for provided size file.
   * @param {MetadataType} sizeMetadata
   * @return {string}
   */
  function renderMetadata(sizeMetadata) {
    const processContainer = (container) => {
      if (container?.hasOwnProperty('metadata')) {
        formatMetadataInPlace(container['metadata']);
      }
      // Strip section_sizes because it is already shown in tree.
      if (container?.hasOwnProperty('section_sizes')) {
        delete container['section_sizes'];
      }
    };
    if (sizeMetadata?.hasOwnProperty('containers')) {
      for (const container of sizeMetadata['containers']) {
        processContainer(container);
      }
    } else {
      // Covers the case if the metadata is in old schema.
      processContainer(sizeMetadata);
    }
    return JSON.stringify(sizeMetadata, null, 2);
  }

  /**
   * Sets the metadata from message to the HTML element.
   * @param {MetadataType} metadata
   */
  function setMetadataContent(metadata) {
    let metadataStr = '';
    if (metadata) {
      const sizeMetadata = metadata['size_file'];
      const sizeMetadataStr = renderMetadata(sizeMetadata);
      if (metadata.hasOwnProperty('before_size_file')) {
        const beforeMetadata = metadata['before_size_file'];
        const beforeMetadataStr = renderMetadata(beforeMetadata);
        metadataStr =
            'Metadata for Before Size File:\n' + beforeMetadataStr + '\n\n\n';
      }
      metadataStr += 'Metadata for Load Size File:\n' + sizeMetadataStr;
    }
    g_el.preMetadataContent.textContent = metadataStr;
  }

  /** @param {!Array<!URL>} urlsToLoad */
  async function performInitialLoad(urlsToLoad) {
    let accessToken = null;
    _progress.setValue(0.1);
    if (requiresAuthentication(urlsToLoad)) {
      accessToken = await fetchAccessToken();
      _progress.setValue(0.2);
    }
    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    const message = await worker.loadAndBuildTree('from-url://', accessToken);
    processLoadTreeResponse(message);
  }

  async function rebuildTree() {
    _progress.setValue(0);
    const message = await window.supersize.worker.buildTree();
    processBuildTreeResponse(message);
    renderAndShowMetricsTree(null);
  }

  g_el.fileUpload.addEventListener('change', async (event) => {
    _progress.setValue(0.1);
    const input = /** @type {HTMLInputElement} */ (event.currentTarget);
    const file = input.files.item(0);
    const fileUrl = URL.createObjectURL(file);

    state.stLoadUrl.set(fileUrl);

    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    const message = await worker.loadAndBuildTree(fileUrl);
    URL.revokeObjectURL(fileUrl);
    processLoadTreeResponse(message);
    // Clean up afterwards so new files trigger event.
    input.value = '';
  });

  g_el.frmOptions.addEventListener('change', event => {
    // Update the tree when options change.
    // Some options update the tree themselves, don't regenerate when those
    // options (marked by "data-dynamic") are changed.
    if (!/** @type {HTMLElement} */ (event.target)
            .dataset.hasOwnProperty('dynamic')) {
      rebuildTree();
    }
  });
  g_el.frmOptions.addEventListener('submit', event => {
    event.preventDefault();
    rebuildTree();
  });

  // Toggles the metadata HTML element on click.
  g_el.divMetadataView.addEventListener('click', () => {
    g_el.preMetadataContent.classList.toggle('active');
  });

  const urlsToLoad = [];
  for (const url of [state.stBeforeUrl.get(), state.stLoadUrl.get()]) {
    if (url)
      urlsToLoad.push(new URL(/** @type {string} */ (url), document.baseURI));
  }
  if (urlsToLoad.length > 0)
    performInitialLoad(urlsToLoad);
}
