// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Base class and helpers for tree navigation UI with collapsible branches and
 * keyboard navigation.
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
    /** @protected @const {!HTMLUListElement} rootElt */
    this.rootElt = rootElt;

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
   * @param {!HTMLAnchorElement} link
   * @return {!HTMLLIElement}
   * @protected
   */
  getTreeItemFromLink(link) {
    // Canonical structure:
    // <li>                       <!-- |treeitem| -->
    //   <a class="node">...</a>  <!-- |link| -->
    //   <ul>...</ul>             <!-- |group| -->
    // </li>
    return /** @type {!HTMLLIElement} */ (link.parentElement);
  }

  /**
   * @param {!HTMLAnchorElement} link
   * @return {!HTMLUListElement}
   * @protected
   */
  getGroupFromLink(link) {
    return /** @type {!HTMLUListElement} */ (link.nextElementSibling);
  }

  /**
   * @param {!HTMLElement} link
   * @param {!Array<DocumentFragment>} childrenElements
   * @protected
   */
  autoExpandAttentionWorthyChild(link, childrenElements) {
    if (childrenElements.length === 1) {
      // Open inner element if it only has a single child; this ensures nodes
      // like "java"->"com"->"google" are opened all at once.
      const node = /** @type {!TreeNodeElement} */ (
          childrenElements[0].querySelector('.node'));
      node.click();
    }
  }

  /**
   * Populates |link| with
   * @param {!HTMLAnchorElement} link
   * @protected
   */
  async expandGroupElement(link) {
    const childrenData = await this.getGroupChildrenData(link);
    const newElements = childrenData.map((data) => this.makeNodeElement(data));
    this.autoExpandAttentionWorthyChild(link, newElements);
    const newElementsFragment = dom.createFragment(newElements);
    requestAnimationFrame(() => {
      this.getGroupFromLink(link).appendChild(newElementsFragment);
    });
  }

  /**
   * Click event handler to expand or close a group node.
   * @param {Event} event
   * @protected
   */
  async toggleGroupElement(event) {
    event.preventDefault();
    const link = /** @type {!HTMLAnchorElement} */ (event.currentTarget);
    const treeitem = this.getTreeItemFromLink(link);
    const group = this.getGroupFromLink(link);

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

  /**
   * Handler for gaining focus relative to other TreeUi instances.
   * @protected
   */
  onTreeFocus() {}

  /**
   * Handler for losing focus relative to other TreeUi instances, i.e., this
   * does NOT fire when non-TreeUi UI elements gain focus.
   * @protected
   */
  onTreeBlur() {}

  /** @public */
  focus() {
    if (TreeUi.activeTreeUi !== this) {
      if (TreeUi.activeTreeUi)
        TreeUi.activeTreeUi.onTreeBlur();
      TreeUi.activeTreeUi = this;
      TreeUi.activeTreeUi.onTreeFocus();
    }
  }

  /** @public */
  init() {
    // Each instance contributes to managing focus / blur dynamics.
    this.rootElt.addEventListener('click', () => this.focus());
  }
}

/** @type {?TreeUi} */
TreeUi.activeTreeUi = null;
