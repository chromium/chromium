// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** TreeVis ********/
class TreeVis {
  constructor(divViewTree) {
    this.el = {
      root: divViewTree,
    };
  }

  clear() {
    this.el.root.innerHTML = '';
  }

  /**
   * Renders the Tree View from parsed Android View data.
   * @param {!Array<!ViewNode>} views An array of parsed Android View, sorted
   *     by pre-order DFS traversal.
   */
  populateViewList(views) {
    const INDENT_PX = 20;
    const LINE_OFFSET_PX = 9;

    this.clear();

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
        const span = makeElt('span\t\u25E2');
        spanTreeToggle.appendChild(span);
        row.appendChild(spanTreeToggle);
      } else {
        row.classList.add('leaf');
      }

      // Label.
      const labelText =
          (view.resourceId) ? view.resourceId : view.className.split('.').pop()
      const label = makeElt(`span.view-label\t${labelText}`);
      label.classList.toggle('no-id', !view.resourceId);
      row.appendChild(label);

      fragment.appendChild(row);
    }

    this.el.root.appendChild(fragment);
  }
}

/******** TreeController ********/
class TreeController {
  /**
   * @param {!MainModel} model The main data model.
   * @param {!TreeVis} vis The tree visualizer.
   */
  constructor(model, vis) {
    this.model = model;
    this.vis = vis;
  }

  populate() {
    this.vis.populateViewList(this.model.views);
  }
}
