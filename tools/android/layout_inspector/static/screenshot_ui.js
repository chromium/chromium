// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** ScreenshotInfoVis Constants ********/

const SS_INFO_SHOW = {
  NONE: 'none',
  SIZE: 'size',
};

/******** ScreenshotInfoVis ********/
/**
 * A floating info panel presenting global device size and current screenshot
 * screen size.
 */
class ScreenshotInfoVis {
  /**
   * @param {!Element} divScreenshotInfo Container element for the info bar.
   * @param {!VisOptions} visOpts Global visualization options.
   */
  constructor(divScreenshotInfo, visOpts) {
    this.el = {
      root: divScreenshotInfo,
      spSizeDevice: divScreenshotInfo.querySelector('.sp-size-device'),
      spSizeScaled: divScreenshotInfo.querySelector('.sp-size-scaled'),
    };
    this.visOpts = visOpts;
  }

  clear() {
    this.el.root.dataset.show = SS_INFO_SHOW.NONE;
  }

  _formatDp(v) {
    return capFixed(this.visOpts.pxToDp(v), 1);
  }

  _showSize() {
    const [ww, wh] = this.visOpts.wDims;
    const [sw, sh] = this.visOpts.sDims.map(Math.round);
    const sZoom = capFixed(this.visOpts.scale * 100, 2);

    const rootW = this.el.root.offsetWidth;
    const showHeading = rootW >= 600;
    const showAux = rootW >= 450;

    this.el.root.classList.toggle('too-narrow', rootW < 300);
    this.el.spSizeDevice.textContent = (showHeading ? 'Device: ' : '') +
        `${ww}x${wh}px` +
        (showAux ? ` (${this._formatDp(ww)}x${this._formatDp(wh)}dp)` : '');
    this.el.spSizeScaled.textContent = (showHeading ? 'Scaled: ' : '') +
        `${sw}x${sh}px` + (showAux ? ` (${sZoom}% Zoom)` : '');
    this.el.root.dataset.show = SS_INFO_SHOW.SIZE;
  }

  update() {
    if (this.visOpts.wDims.w > 0 && this.visOpts.wDims.h > 0) {
      this._showSize();
    } else {
      this.clear();
    }
  }
}

/******** ScreenshotVis ********/
/**
 * Renders the device's image into an HTML canvas.
 */
class ScreenshotVis {
  /**
   * @param {!Element} divScreenshot Viewport element for the screenshot.
   * @param {!Element} divScreenshotInfo Info bar element.
   * @param {!VisOptions} visOpts Global visualization options.
   */
  constructor(divScreenshot, divScreenshotInfo, visOpts) {
    this.el = {
      root: divScreenshot,
      inner: divScreenshot.querySelector('.screenshot-inner'),
      canvBase: divScreenshot.querySelector('.canv-base'),
    };
    this.visOpts = visOpts;

    this.ctx = this.el.canvBase.getContext('2d');

    this.screenshotInfoVis =
        new ScreenshotInfoVis(divScreenshotInfo, this.visOpts);
  }

  clear() {
    this.ctx.clearRect(0, 0, this.el.canvBase.width, this.el.canvBase.height);
    this.screenshotInfoVis.clear();
  }

  _applyRescale() {
    const rect = this.el.root.getBoundingClientRect();
    const vpDims = new Dims2D(rect.width, rect.height);

    // Standard fit and clear forced scroll bars.
    this.visOpts.updateGeometry(vpDims);
    delete this.el.root.dataset.forceScroll;

    if (this.visOpts.isZoomFit()) {
      // For Zoom Fit (WLOG under horizontal layout), the scale is the maximal
      // value that avoids horizontal scrollbar. However, if vertical scrollbar
      // shows then it would consume horizontal space, which then causes the
      // horizontal scrollbar to show! The code here detects this "induced
      // scrollbar" case, shrinks `vpDims` and retries. But for the boundary
      // case, the shrinkage can render scrollbars unneeded again. This leads to
      // awkward gaps! This is addressed by forcing the vertical scrollbar to
      // appear. Under vertical layout, things are similar.
      const {sDims, layoutMode} = this.visOpts;
      if (layoutMode.orientation === ORIENTATION.HORIZ) {
        if (sDims.h > vpDims.h) {
          vpDims.w = Math.max(0, vpDims.w - getScrollbarThickness());
          this.visOpts.updateGeometry(vpDims);
          this.el.root.dataset.forceScroll = 'y';
        }
      } else {  // layoutMode.orientation === ORIENTATION.VERT
        if (sDims.w > vpDims.w) {
          vpDims.h = Math.max(0, vpDims.h - getScrollbarThickness());
          this.visOpts.updateGeometry(vpDims);
          this.el.root.dataset.forceScroll = 'x';
        }
      }
    }

    const st = this.el.inner.style;
    const [sw, sh] = this.visOpts.sDims;
    st.setProperty('--screenshot-width', `${Math.round(sw)}px`);
    st.setProperty('--screenshot-height', `${Math.round(sh)}px`);
  }

  updateScaleAndRefresh(needToRescale) {
    if (needToRescale) {
      this._applyRescale();
    }
    this.screenshotInfoVis.update();
  }

  updateZoom() {
    this.updateScaleAndRefresh(true);
  }
  /**
   * @param {!Image} imgScreenshot
   */
  setScreenshot(imgScreenshot) {
    const el = this.el;
    const {width, height} = imgScreenshot;
    el.canvBase.width = width;
    el.canvBase.height = height;
    this.ctx.drawImage(imgScreenshot, 0, 0);

    this.updateZoom();
  }
}

/******** ScreenshotController ********/
class ScreenshotController {
  /**
   * @param {!MainModel} model The main data model.
   * @param {!Element} divScreenshot Viewport element for the screenshot.
   * @param {!Element} divScreenshotInfo Info bar element.
   * @param {!Object} callbacks
   * @param {function()} callbacks.onResize Invoked when the viewport is
   *     resized.
   */
  constructor(model, divScreenshot, divScreenshotInfo, {onResize}) {
    this.model = model;
    this.vis =
        new ScreenshotVis(divScreenshot, divScreenshotInfo, this.model.visOpts);
    this.el = this.vis.el;
    this.visOpts = this.model.visOpts;
    this.onResize = onResize;

    this._bindAll();
  }

  clear() {
    this.vis.clear();
  }

  updateZoom() {
    this.vis.updateZoom();
  }

  initScreenshot() {
    this.vis.setScreenshot(this.model.imgScreenshot);
  }

  _bindViewportResize() {
    const observer = new ResizeObserver(() => {
      if (this.model.isLoaded) {
        this.vis.updateScaleAndRefresh(this.visOpts.isZoomFit());
      }
      this.onResize();
    });
    observer.observe(this.el.root);
  }

  _bindAll() {
    this._bindViewportResize();
  }
}
