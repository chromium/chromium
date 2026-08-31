// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

const MIN_PANE_DIMS = new Dims2D(50, 150);

/******** LayoutController ********/
class LayoutController {
  constructor(model, divMain, divPaneScreenshot, divMainSplitter, hintCtrl) {
    this.visOpts = model.visOpts;

    this.divMain = divMain;
    this.divPaneScreenshot = divPaneScreenshot;
    this.divMainSplitter = divMainSplitter;
    this.hintCtrl = hintCtrl;

    this.activeDragState = null;

    this.divMainSplitterDragHandler = new DragHandler(divMainSplitter, {
      pointerStyle: this.visOpts.layoutMode.cursor,
      onDragStart: (e) => this.handleDragStart(e),
      onDrag: (e, dx, dy) => this.handleDrag(e, dx, dy),
      onDragEnd: (e) => this.handleDragEnd(e, false),
      onDragCancel: (e) => this.handleDragEnd(e, true),
    });

    this.divMainSplitter.addEventListener('pointerenter', () => {
      this.hintCtrl.setHint(HINT.LAYOUT_SPLITTER);
    });
    this.divMainSplitter.addEventListener('pointerleave', () => {
      this.hintCtrl.clear();
    });

    this.setLayoutMode(LayoutMode.LEFT);
  }

  _readPaneScreenshotRatio() {
    // If flex-basis is not explicitly set, assume default 50% from CSS.
    const basis = this.divPaneScreenshot.style.flexBasis;
    return (basis && basis.endsWith('%')) ? parseFloat(basis) / 100 : 0.5;
  }

  /** @param {!LayoutMode} mode */
  _getParentSizeAlongMode(mode) {
    const {width, height} = this.divMain.getBoundingClientRect();
    return new Dims2D(width, height).sizeAlong(mode.dir);
  }

  /**
   * @param {!LayoutMode} mode
   * @param {number} ratio
   */
  _computePaneScreenshotRatio(mode, ratio) {
    const parentSize = this._getParentSizeAlongMode(mode);
    const minSize = MIN_PANE_DIMS.sizeAlong(mode.dir);
    const ratioBound =
        (parentSize <= 0) ? 0.5 : Math.min(minSize / parentSize, 0.5);
    return clip(ratioBound, ratio, 1.0 - ratioBound);
  }

  _setPaneScreenshotRatio(ratio) {
    const safeRatio =
        this._computePaneScreenshotRatio(this.visOpts.layoutMode, ratio);
    this.divPaneScreenshot.style.flexBasis = `${safeRatio * 100}%`;
  }

  handleDragStart(e) {
    this.activeDragState = {
      startRatio: this._readPaneScreenshotRatio(),
    };
  }

  handleDrag(e, dx, dy) {
    const state = this.activeDragState;
    if (!state) return;

    // Resize along axis based on ratio change.
    const parentSize = this._getParentSizeAlongMode(this.visOpts.layoutMode);
    if (parentSize > 0) {
      const dSize = -this.visOpts.layoutMode.dir.dotxy(dx, dy);
      this._setPaneScreenshotRatio(state.startRatio + dSize / parentSize);
    }
  }

  handleDragEnd(e, isCancel) {
    const state = this.activeDragState;
    if (!state) return;

    if (isCancel) this._setPaneScreenshotRatio(state.startRatio);
    this.activeDragState = null;
  }

  setLayoutMode(mode) {
    this.visOpts.layoutMode = mode;
    this.divMain.dataset.layout = mode.name;
    this.divMainSplitterDragHandler.setPointerStyle(mode.cursor);
  }
}
