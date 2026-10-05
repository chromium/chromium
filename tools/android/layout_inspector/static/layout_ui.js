// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

const MIN_PANE_DIMS = new Dims2D(50, 150);

/******** LayoutController ********/
class LayoutController {
  constructor(model, divMain, divPaneScreenshot, divMainSplitter, divScreenshot,
              divScreenshotInfo, hintCtrl) {
    this.model = model;
    this.visOpts = model.visOpts;

    this.el = {
      divMain,
      divPaneScreenshot,
      divScreenshotInfo,
      divMainSplitter,
      divScreenshot,
    };
    this.hintCtrl = hintCtrl;

    this.activeDragState = null;

    this.mainSplitterDragHandler = new DragHandler(this.el.divMainSplitter, {
      pointerStyle: this.visOpts.layoutMode.cursor,
      onDragStart: (e) => this._handleDragStart(e),
      onDrag: (e, dx, dy) => this._handleDrag(e, dx, dy),
      onDragEnd: (e) => this._handleDragEnd(e, false),
      onDragCancel: (e) => this._handleDragEnd(e, true),
      onDoubleClick: (e) => this._autoResize(),
    });

    this.el.divMainSplitter.addEventListener('pointerenter', () => {
      this.hintCtrl.setHint(HINT.LAYOUT_SPLITTER);
    });
    this.el.divMainSplitter.addEventListener('pointerleave', () => {
      this.hintCtrl.clear();
    });

    this.setLayoutMode(LayoutMode.LEFT);
  }

  /** @param {!LayoutMode} mode */
  setLayoutMode(mode) {
    this.visOpts.layoutMode = mode;
    this.el.divMain.dataset.layout = mode.name;
    this.mainSplitterDragHandler.setPointerStyle(mode.cursor);
  }

  /** @param {!LayoutMode} mode */
  _getParentSizeAlongMode(mode) {
    const {width, height} = this.el.divMain.getBoundingClientRect();
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
    this.el.divPaneScreenshot.style.flexBasis = `${safeRatio * 100}%`;
  }

  _readPaneScreenshotRatio() {
    // If flex-basis is not explicitly set, assume default 50% from CSS.
    const basis = this.el.divPaneScreenshot.style.flexBasis;
    return (basis && basis.endsWith('%')) ? parseFloat(basis) / 100 : 0.5;
  }

  _handleDragStart(e) {
    this.activeDragState = {
      startRatio: this._readPaneScreenshotRatio(),
    };
  }

  _handleDrag(e, dx, dy) {
    const state = this.activeDragState;
    if (!state) return;

    // Resize along axis based on ratio change.
    const parentSize = this._getParentSizeAlongMode(this.visOpts.layoutMode);
    if (parentSize > 0) {
      const dSize = -this.visOpts.layoutMode.dir.dotxy(dx, dy);
      this._setPaneScreenshotRatio(state.startRatio + dSize / parentSize);
    }
  }

  _handleDragEnd(e, isCancel) {
    const state = this.activeDragState;
    if (!state) return;

    if (isCancel) this._setPaneScreenshotRatio(state.startRatio);
    this.activeDragState = null;
  }

  _autoResize() {
    if (!this.model.isLoaded) return;

    const layoutMode = this.visOpts.layoutMode;
    const parentSizePx = this._getParentSizeAlongMode(layoutMode);
    if (parentSizePx <= 0) return;

    const imgDims = this.visOpts.getAutoResizeDims();
    const rect = this.el.divScreenshot.getBoundingClientRect();
    const thickness = getScrollbarThickness();

    // 1. Primary Scrollbar Detection based on stable container dims.
    let hScrollNeeded = imgDims.w > rect.width;
    let vScrollNeeded = imgDims.h > rect.height;

    // 2. Ripple Effect Detection.
    // If one scrollbar appears, it might trigger the other by eating space.
    if (hScrollNeeded && !vScrollNeeded) {
      if (imgDims.h > rect.height - thickness) vScrollNeeded = true;
    } else if (vScrollNeeded && !hScrollNeeded) {
      if (imgDims.w > rect.width - thickness) hScrollNeeded = true;
    }

    // 3. Calculate total overhead based on layout axis.
    let overhead = 0;
    if (layoutMode.orientation === ORIENTATION.HORIZ) {
      if (vScrollNeeded) overhead += thickness;
    } else {  // layoutMode.orientation === ORIENTATION.VERT
      overhead += this.el.divScreenshotInfo.offsetHeight;
      if (hScrollNeeded) overhead += thickness;
    }

    const targetSizePx = imgDims.sizeAlong(layoutMode.dir) + overhead;
    this._setPaneScreenshotRatio(targetSizePx / parentSizePx);
  }
}
