// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** ScreenshotInfoVis Constants ********/

const SS_INFO_SHOW = {
  NONE: 'none',
  POINTER: 'pointer',
  SIZE: 'size',
};

/******** ScreenshotInfoVis ********/
/**
 * A floating info panel presenting global device size and current screenshot
 * screen size, or on hover, current pointer coordinates and pixel colors.
 */
class ScreenshotInfoVis {
  /**
   * @param {!Element} divScreenshotInfo Container element for the info bar.
   * @param {!VisOptions} visOpts
   */
  constructor(divScreenshotInfo, visOpts) {
    this.el = {
      root: divScreenshotInfo,
      spSizeDevice: divScreenshotInfo.querySelector('.sp-size-device'),
      spSizeScaled: divScreenshotInfo.querySelector('.sp-size-scaled'),
      spPointerPx: divScreenshotInfo.querySelector('.sp-pointer-px'),
      spPointerDp: divScreenshotInfo.querySelector('.sp-pointer-dp'),
      spColorPreview: divScreenshotInfo.querySelector('.sp-color-preview'),
      spColorHex: divScreenshotInfo.querySelector('.sp-color-hex'),
    };
    this.visOpts = visOpts;
  }

  clear() {
    this.el.root.dataset.show = SS_INFO_SHOW.NONE;
  }

  _formatDp(v) {
    return capFixed(this.visOpts.pxToDp(v), 1);
  }

  _hex2(c) {
    return (c ?? 0).toString(16).toUpperCase().padStart(2, '0');
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

  /**
   * @param {!ScreenshotPointer} pointer
   * @param {!ImageData} imageData
   */
  _showPointer(pointer, imageData) {
    const [ww, wh] = this.visOpts.wDims;
    const [wx, wy] = pointer.pt.map(Math.floor);

    this.el.spPointerPx.textContent = `(${wx}, ${wy})px`;
    this.el.spPointerDp.textContent =
        `(${this._formatDp(wx)}, ${this._formatDp(wy)})dp`;

    if (wx >= 0 && wx < ww && wy >= 0 && wy < wh) {
      const data = imageData.data;
      const i = (wy * ww + wx) * 4;
      const [r, g, b] = [data[i], data[i + 1], data[i + 2]];
      const hex = `#${this._hex2(r)}${this._hex2(g)}${this._hex2(b)}`;

      this.el.spColorPreview.style.backgroundColor = hex;
      this.el.spColorHex.textContent = hex;
    } else {
      this.el.spColorPreview.style.backgroundColor = 'transparent';
      this.el.spColorHex.textContent = '';
    }
    this.el.root.dataset.show = SS_INFO_SHOW.POINTER;
  }

  /**
   * @param {?ScreenshotPointer=} pointer
   * @param {?ImageData=} imageData
   */
  update(pointer = null, imageData = null) {
    if (pointer != null && imageData != null) {
      this._showPointer(pointer, imageData);
    } else if (this.visOpts.wDims.w > 0 && this.visOpts.wDims.h > 0) {
      this._showSize();
    } else {
      this.clear();
    }
  }
}

/******** ScreenshotPointer ********/
/**
 * Tracks input device state on the screen.
 *
 * Normalizes `PointerEvent` offsets against device-independent scaling logic
 * via `s2wDevice()`, ensuring the pointer coordinates always live in the World
 * domain.
 */
class ScreenshotPointer {
  /**
   * @param {!Element} innerEl
   * @param {!VisOptions} visOpts
   */
  constructor(innerEl, visOpts) {
    this.pt = new PointXY(-1, -1);
    this.innerEl = innerEl;
    this.visOpts = visOpts;
    this.isPressed = false;
    this.pointerId = null;
  }

  /**
   * @param {!PointerEvent} e
   * @return {boolean} False if ignored due to another active pressed pointer.
   */
  updateFromPointerEvent(e) {
    if (this.pointerId == null) {
      // Imprint on the very first pointer seen, regardless of type.
      this.pointerId = e.pointerId;
    } else if (this.pointerId !== e.pointerId) {
      // If a new pointer arrives and the old one is no longer 'pressed', allow
      // the new pointer to take over.
      if (!this.isPressed) {
        this.pointerId = e.pointerId;
      } else {
        // Ignore event from a different pointer if one is active.
        return false;
      }
    }
    this.isPressed = (e.buttons !== 0);
    const rect = this.innerEl.getBoundingClientRect();
    this.pt.x = this.visOpts.s2wDevice(e.clientX - rect.left);
    this.pt.y = this.visOpts.s2wDevice(e.clientY - rect.top);
    return true;
  }

  leave() {
    this.isPressed = false;
    this.pointerId = null;
  }
}

/******** ScreenshotVis ********/
/**
 * Renders the device's image into an HTML canvas and maintains the SVG overlay
 * container synced in size via `setZoom()`.
 */
class ScreenshotVis {
  /**
   * @param {!Element} divScreenshot Viewport element for the screenshot.
   * @param {!Element} divScreenshotInfo Info bar element.
   * @param {!MainModel} model
   */
  constructor(divScreenshot, divScreenshotInfo, model) {
    this.el = {
      root: divScreenshot,
      inner: divScreenshot.querySelector('.screenshot-inner'),
      canvBase: divScreenshot.querySelector('.canv-base'),
      svgMarkup: divScreenshot.querySelector('.svg-markup'),
    };
    this.model = model;
    this.visOpts = model.visOpts;

    this.ctx = this.el.canvBase.getContext('2d');

    this.screenshotInfoVis =
        new ScreenshotInfoVis(divScreenshotInfo, this.visOpts);

    this.hoverHighlight = new ViewHighlight();
    this.hoverHighlight.root.classList.add(MARKUP_STATUS.HOVER);

    g_svg.edit(this.el.svgMarkup).append(this.hoverHighlight.root);

    this.screenshotImageData = null;

    /** @type {?number} Nullable index of the View with hover visuals. */
    this.hoverIndex = null;
  }

  clear() {
    this.hoverIndex = null;
    this.hoverHighlight.setVisible(false);
    this.el.inner.classList.remove('active');
    this.ctx.clearRect(0, 0, this.el.canvBase.width, this.el.canvBase.height);
    this.screenshotImageData = null;
    this.screenshotInfoVis.clear();
  }

  /**
   * @param {!Image} imgScreenshot
   */
  setScreenshot(imgScreenshot) {
    const el = this.el;
    const {width, height} = imgScreenshot;
    el.inner.classList.add('active');
    el.canvBase.width = width;
    el.canvBase.height = height;
    this.ctx.drawImage(imgScreenshot, 0, 0);
    this.screenshotImageData = (width > 0 && height > 0)
        ? this.ctx.getImageData(0, 0, width, height)
        : null;
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
    const [sw, sh] = this.visOpts.sDims.map(Math.round);
    st.setProperty('--screenshot-width', `${sw}px`);
    st.setProperty('--screenshot-height', `${sh}px`);
    g_svg.edit(this.el.svgMarkup).viewBoxDims(sw, sh);
  }

  /**
   * Shows / hides hover visuals based on `hoverIndex`.
   * @param {boolean} enable
   */
  _syncHover(enable) {
    if (this.hoverIndex == null) return;

    if (enable) {
      const view = this.model.views[this.hoverIndex];
      this.hoverHighlight.setRectAndRedraw(view.rect, this.visOpts);
    }
    this.hoverHighlight.setVisible(enable);
  }

  /**
   * Transactional wrapper to temporarily remove hover visuals, run state
   * changes that would affect DOM and Vis states, then restore (if needed).
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

  updateAllHighlights() {
    // Placeholder: Upcoming CLs will update fill style and selection
    // highlights here (hover is re-synced by the enclosing transaction).
  }

  updateScale() {
    this._applyRescale();
  }
}

/******** ScreenshotController ********/
class ScreenshotController {
  /**
   * @param {!MainModel} model
   * @param {!Element} divScreenshot Viewport element for the screenshot.
   * @param {!Element} divScreenshotInfo Info bar element.
   * @param {!Object} callbacks
   * @param {function()} callbacks.onResize Invoked when the viewport is
   *     resized.
   */
  constructor(model, divScreenshot, divScreenshotInfo, {onResize}) {
    this.model = model;
    this.vis = new ScreenshotVis(divScreenshot, divScreenshotInfo, this.model);
    this.el = this.vis.el;
    this.visOpts = this.model.visOpts;
    this.onResize = onResize;

    this.pointer = new ScreenshotPointer(this.el.inner, this.visOpts);

    this._bindAll();
  }

  _updateScaleAndInfo() {
    this.vis.withTransaction(() => {
      this.vis.updateScale();
      this.vis.screenshotInfoVis.update();
    });
  }

  clear() {
    this.pointer.leave();
    this.vis.clear();
  }

  initScreenshot() {
    this.vis.setScreenshot(this.model.imgScreenshot);
    this._updateScaleAndInfo();
  }

  setHover(index) {
    this.vis.setHover(index);
  }

  updateAllHighlights() {
    this.vis.withTransaction(() => {
      this.vis.updateAllHighlights();
    });
  }

  updateZoom() {
    this._updateScaleAndInfo();
  }

  _clearHoverState() {
    this.pointer.leave();
    this.vis.screenshotInfoVis.update();
  }

  _bindHandlers(elt, handlers) {
    const dispatch = (e) => {
      return this.model.isLoaded ? handlers[e.type](e) : false;
    };

    for (const type of Object.keys(handlers)) {
      elt.addEventListener(type, dispatch);
    }
  }

  _bindScreenshotEvents() {
    const handlers = {};

    handlers['pointerleave'] = (e) => {
      if (!this.pointer.updateFromPointerEvent(e)) return;

      this._clearHoverState();
    };

    handlers['pointermove'] = (e) => {
      if (!this.pointer.updateFromPointerEvent(e)) return;

      this.vis.screenshotInfoVis.update(this.pointer,
                                        this.vis.screenshotImageData);
    };

    this._bindHandlers(this.el.inner, handlers);
  }

  _bindViewportResize() {
    const observer = new ResizeObserver(() => {
      if (this.model.isLoaded) {
        this.vis.withTransaction(() => {
          if (this.visOpts.isZoomFit()) this.vis.updateScale();
          this.vis.screenshotInfoVis.update();
        });
      }
      this.onResize();
    });
    observer.observe(this.el.root);
  }

  _bindAll() {
    this._bindScreenshotEvents();
    this._bindViewportResize();
  }
}
