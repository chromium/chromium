// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** TreeVis ********/
/**
 * Renders and manages the interactive, hierarchical DOM-based View list.
 *
 * Employs a Nearest Visible Ancestor (NVA) map to handle deep element
 * highlights. If a selected or hovered View is hidden inside a collapsed parent
 * node, the NVA map resolves the closest visible parent, allowing the parent
 * row to render an active ancestor highlight (e.g., 'hides-selection',
 * 'hides-hover').
 */
class TreeVis {
  /**
   * @param {!Element} divViewTree
   * @param {!MainModel} model
   */
  constructor(divViewTree, model) {
    this.el = {
      root: divViewTree,
    };
    this.model = model;

    /** @type {!Array<!Element>} View Row (by index) DOM element cache. */
    this.viewRows = [];
    /** @type {!Array<boolean>} View Row (by index) expansion state. */
    this.expandedMap = [];
    /** @type {!Int32Array} View Row (by index) NVA (possibly self). */
    this.nvaMap = new Int32Array(0);
    /** @type {?number} Nullable index of the View Row with hover visuals. */
    this.hoverIndex = null;
    /**
     * @type {!Set<number>} Set of collapsed View Rows (as indices) that have a
     *     strict descendant that is selected.
     */
    this.hidesSelection = new Set();
  }

  clear() {
    // No need to call `_syncHover(false)`.
    this.el.root.innerHTML = '';
    this.hidesSelection.clear();
    this.hoverIndex = null;
    this.nvaMap = new Int32Array(0);
    this.expandedMap.length = 0;
    this.viewRows.length = 0;
  }

  _recalcNVA() {
    // Calculate the Nearest Visible Ancestor (NVA) for every node in a single
    // pass. Because the views array is pre-sorted in pre-order DFS traversal, a
    // parent's visibility and NVA are guaranteed to be computed before we visit
    // its children. This allows us to propagate visibility state down the tree
    // in O(N) time without any recursion or back-tracking!
    for (const node of this.model.views) {
      if (!node.parent) {
        this.nvaMap[node.index] = node.index;
      } else {
        const pNva = this.nvaMap[node.parent.index];
        const isParentVisible = (pNva === node.parent.index);
        const isParentExpanded = this.expandedMap[node.parent.index];
        this.nvaMap[node.index] =
            (isParentVisible && isParentExpanded) ? node.index : pNva;
      }
    }
  }

  _updateHidesSelection() {
    // Clear all selection highlights.
    for (const index of this.hidesSelection) {
      this.viewRows[index].classList.remove('hides-selection');
    }
    this.hidesSelection.clear();

    // Mark collapsed ancestors using NVA map.
    for (const index of this.model.engaged.sels.visitSorted()) {
      // Find the NVA of each selected `ViewNode`, represented by `index`. If
      // same as self, then the View Row is visible; otherwise it's hidden, and
      // its NVA's View should be added.
      const nvaIndex = this.nvaMap[index];
      if (nvaIndex !== index) {
        this.hidesSelection.add(nvaIndex);
      }
    }

    // Set all selection highlights.
    for (const index of this.hidesSelection) {
      this.viewRows[index].classList.add('hides-selection');
    }
  }

  updateVisibility() {
    this._recalcNVA();

    for (const view of this.model.views) {
      const row = this.viewRows[view.index];

      // Visibility (Row).
      // Node is visible if its NVA is itself.
      const isVisible = (this.nvaMap[view.index] === view.index);
      row.classList.toggle('hidden', !isVisible);

      // Toggle Icon (Arrow).
      if (view.children.length > 0) {
        row.classList.toggle('expanded', this.expandedMap[view.index]);
      }
    }

    this._updateHidesSelection();
  }

  updateAllSelections() {
    for (const row of this.viewRows) {
      row.classList.remove('selected-view');
    }
    for (const i of this.model.engaged.sels.visitSorted()) {
      this.viewRows[i].classList.add('selected-view');
    }

    this._updateHidesSelection();
  }

  /** Renders the Tree View from parsed Android View data. */
  populateViewList() {
    const INDENT_PX = 20;
    const LINE_OFFSET_PX = 9;

    const {views, windowInfos} = this.model;

    this.clear();
    // Initially expand all View Rows.
    this.expandedMap = new Array(views.length).fill(true);
    // Collapse specific Window View Rows (e.g., system windows).
    for (const {view, initiallyExpanded} of windowInfos) {
      this.expandedMap[view.index] = initiallyExpanded;
    }

    this.nvaMap = new Int32Array(views.length);
    this.viewRows = new Array(views.length);

    // Look-up to determine whether a View (identified by index) should have a
    // Tree Line drawn to the next sibling.
    const hasNextSibling = new Array(views.length).fill(true);
    if (windowInfos.length > 0) {
      hasNextSibling[windowInfos.at(-1).view.index] = false;
    } else if (views.length > 0) {
      hasNextSibling[0] = false;
    }
    for (const view of views) {
      if (view.children && view.children.length > 0) {
        hasNextSibling[view.children.at(-1).index] = false;
      }
    }

    const fragment = document.createDocumentFragment();

    for (const view of views) {
      const row = makeElt(`div.view-row@data-index=${view.index}`);

      // Indentation and Tree Lines via single-element background gradients.
      const lines = [];
      for (let i = 0; i < view.depth; i++) {
        let ancestor = view.parent;
        for (let j = 0; j < view.depth - 1 - i; j++) {
          ancestor = ancestor.parent;
        }
        if (ancestor && hasNextSibling[ancestor.index]) {
          const start = i * INDENT_PX + LINE_OFFSET_PX;
          const end = start + 1;
          lines.push(`linear-gradient(to right, #0000 ${start}px, #0003 ${
              start}px, #0003 ${end}px, #0000 ${end}px)`);
        }
      }

      if (lines.length > 0) {
        row.style.backgroundImage = lines.join(', ');
      }
      row.style.paddingLeft = `${view.depth * INDENT_PX}px`;

      // Tree Toggle for internal View nodes, or placeholder for leaves.
      const isInternal = view.children.length > 0;
      if (isInternal) {
        const spanTreeToggle = makeElt('span.tree-toggle');
        const span = makeElt('span');
        spanTreeToggle.appendChild(span);

        // Initial state set in updateVisibility().
        row.appendChild(spanTreeToggle);
      } else {
        row.classList.add('leaf');
      }

      // Label.
      const labelText =
          (view.resourceId) ? view.resourceId : view.className.split('.').pop();
      const label = makeElt(`span.view-label\t${labelText}`);
      label.classList.toggle('no-id', !view.resourceId);
      row.appendChild(label);

      fragment.appendChild(row);
      this.viewRows[view.index] = row;
    }

    // Apply Window-specific styling (e.g., bold text).
    for (const {view} of windowInfos) {
      this.viewRows[view.index].classList.add('is-window');
    }

    this.el.root.appendChild(fragment);

    this.updateVisibility();
    this.updateAllSelections();
  }

  toggleRowExpansion(index, includeDescendants) {
    const views = this.model.views;
    const targetState = !this.expandedMap[index];
    this.expandedMap[index] = targetState;

    if (includeDescendants) {
      const depthLimit = views[index].depth;
      // Since the tree is flat and pre-sorted in DFS order, all descendant
      // nodes reside consecutively after the parent. We can simply scan
      // forward, updating states, and stop immediately when we encounter a node
      // whose depth is less than or equal to the starting node's depth.
      for (let i = index + 1; i < views.length && views[i].depth > depthLimit;
           i++) {
        this.expandedMap[views[i].index] = targetState;
      }
    }
  }

  /**
   * Shows / hides hover visuals based on `hoverIndex`.
   * @param {boolean} enable
   */
  _syncHover(enable) {
    if (this.hoverIndex == null) return;

    this.viewRows[this.hoverIndex].classList.toggle('hovered-view', enable);
    const nvaIndex = this.nvaMap[this.hoverIndex];
    if (nvaIndex !== this.hoverIndex) {
      this.viewRows[nvaIndex].classList.toggle('hides-hover', enable);
    }
  }

  /**
   * Transactional wrapper to temporarily remove hover visuals, run state
   * changes that would affect DOM and Vis states, then restore (if needed).
   * In particular, this allows ancestor hover highlights to gracefully update
   * as View Rows dynamically show / hide, and `nvaMap` updates.
   * @template T
   * @param {function(): T} fun The state mutation transaction callback.
   * @return {T}
   */
  withTransaction(fun) {
    this._syncHover(false);
    try {
      return fun();
    } finally {
      this._syncHover(true);
    }
  }

  /**
   * Assigns the stored hover index.
   * @param {?number} index
   */
  setHover(index) {
    if (this.hoverIndex === index) return;

    this.withTransaction(() => {
      this.hoverIndex = index;
    });
  }
}

/******** TreeController ********/
/** Orchestrates UI interaction events for the View Tree. */
class TreeController {
  /**
   * @param {!MainModel} model
   * @param {!Element} divViewTree Container Element for the View Tree.
   * @param {!Object} callbacks Interaction callbacks to the owner.
   * @param {function(?number)} callbacks.onHover
   * @param {function(number)} callbacks.onToggleSelection
   */
  constructor(model, divViewTree, {onHover, onToggleSelection}) {
    this.model = model;
    this.vis = new TreeVis(divViewTree, this.model);
    this.onHover = onHover;
    this.onToggleSelection = onToggleSelection;

    this.bindAll();
  }

  clear() {
    this.vis.clear();
  }

  setHover(index) {
    this.vis.setHover(index);
  }

  /**
   * @param {Number} index Index of to View Row expand / collapse.
   * @param {boolean} includeDescendants Whether to expand / collapse all
   *     descendant nodes.
   */
  _onToggleExpansion(index, includeDescendants) {
    this.vis.withTransaction(() => {
      this.vis.toggleRowExpansion(index, includeDescendants);
      this.vis.updateVisibility();
    });
  }

  populate() {
    this.vis.populateViewList();
  }

  updateAllSelections() {
    this.vis.withTransaction(() => {
      this.vis.updateAllSelections();
    });
  }

  bindAll() {
    const root = this.vis.el.root;

    // Resolves the View Row index for an event target within the tree. Examine
    // `relatedTarget` to reject spurious `pointerover` / `pointerout`
    // transitions between `.view-row` and its internal `.tree-toggle`.
    const getRowIndex = (e) => {
      const row = e.target.closest('.view-row');
      if (!row || (e.relatedTarget && row.contains(e.relatedTarget))) {
        return null;
      }
      return parseInt(row.dataset.index, 10);
    };

    root.addEventListener('pointerover', (e) => {
      const index = getRowIndex(e);
      if (index !== null) this.onHover(index);
    });

    root.addEventListener('pointerout', (e) => {
      const index = getRowIndex(e);
      // Skip unhovering if moving directly into another View Row.
      if (index !== null && !e.relatedTarget?.closest('.view-row')) {
        this.onHover(null);
      }
    });

    root.addEventListener('click', (e) => {
      const index = getRowIndex(e);
      if (index !== null) {
        if (e.target.classList.contains('tree-toggle')) {
          this._onToggleExpansion(index, e.altKey);
        } else {
          this.onToggleSelection(index);
        }
      }
    });
  }
}
