// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** TreeVis ********/
/** Renders and manages the interactive, hierarchical DOM-based View list. */
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
    /** @type {?number} Nullable index of the View Row with hover visuals. */
    this.hoverIndex = null;
  }

  clear() {
    // No need to call `_renderHover(false)`.
    this.el.root.innerHTML = '';
    this.hoverIndex = null;
    this.expandedMap.length = 0;
    this.viewRows.length = 0;
  }

  updateVisibility() {
    for (const view of this.model.views) {
      const row = this.viewRows[view.index];

      // Visibility (Row).
      // Node is visible if and only if all of its ancestors are expanded.
      // NOTE: This parent-walk is a simple O(depth) implementation. We'll
      // optimize this to O(1) in a follow-up CL using a Nearest Visible
      // Ancestor (NVA) map once hover/selection highlights are introduced.
      let isVisible = true;
      for (let curView = view.parent; curView; curView = curView.parent) {
        if (!this.expandedMap[curView.index]) {
          isVisible = false;
          break;
        }
      }
      row.classList.toggle('hidden', !isVisible);

      // Toggle Icon (Arrow).
      if (view.children.length > 0) {
        row.classList.toggle('expanded', this.expandedMap[view.index]);
      }
    }
  }

  /** Renders the Tree View from parsed Android View data. */
  populateViewList() {
    const INDENT_PX = 20;
    const LINE_OFFSET_PX = 9;

    const views = this.model.views;

    this.clear();
    // Fill to true so that every View Row is shown at the start.
    this.expandedMap = new Array(views.length).fill(true);
    this.viewRows = new Array(views.length);

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
        if (ancestor && !ancestor.isLastChild) {
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

    this.el.root.appendChild(fragment);

    this.updateVisibility();
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
  _renderHover(enable) {
    if (this.hoverIndex == null) return;

    this.viewRows[this.hoverIndex].classList.toggle('hovered-view', enable);
  }

  /**
   * Transactional wrapper to temporarily remove hover visuals, run state
   * changes that would affect DOM and Vis states, then restore (if needed).
   * @param {function()} fun The state mutation transaction callback.
   */
  withTransaction(fun) {
    this._renderHover(false);
    fun();
    this._renderHover(true);
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
   */
  constructor(model, divViewTree, {onHover}) {
    this.model = model;
    this.vis = new TreeVis(divViewTree, this.model);
    this.onHover = onHover;

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

  bindAll() {
    const root = this.vis.el.root;
    const views = this.model.views;

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
          // TODO: Handle direct Tree Row click for selection.
        }
      }
    });
  }
}
