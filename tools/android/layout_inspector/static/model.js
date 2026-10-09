// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

const ZOOM_LEVELS = [
  {title: 'Fit', scale: null},
  {title: '10%', scale: 1 / 10},
  {title: '25%', scale: 1 / 4},
  {title: '33.3%', scale: 1 / 3},
  {title: '50%', scale: 1 / 2},
  {title: '66.7%', scale: 2 / 3},
  {title: '75%', scale: 3 / 4},
  {title: '100%', scale: 1},
  {title: '150%', scale: 1.5},
  {title: '200%', scale: 2},
  {title: '300%', scale: 3},
  {title: '400%', scale: 4},
];
const ZOOM_FIT_INDEX = 0;

/******** LayoutMode ********/
/**
 * Defines the direction and interaction properties for a layout configuration
 * (e.g., `LEFT` is for Screenshot on the left, Tree on the right).
 */
class LayoutMode {
  /**
   * @param {string} name e.g., 'left', 'top'. Used for data-layout attribute.
   *     Fundamentally this represents the position of Screenshot panel.
   * @param {!PointXY} dir The primary axis direction vector.
   */
  constructor(name, dir) {
    this.name = name;
    this.dir = dir;
    Object.freeze(this);
  }

  /**
   * Returns the layout mode most aligned with the given direction vector.
   * @param {number} vx
   * @param {number} vy
   * @param {LayoutMode=} fallbackMode Mode to return if the vector is zero.
   * @return {!LayoutMode}
   */
  static fromVector(vx, vy, fallbackMode = LayoutMode.LEFT) {
    if (vx === 0 && vy === 0) return fallbackMode;

    if (Math.abs(vx) >= Math.abs(vy)) {
      return (vx < 0) ? LayoutMode.LEFT : LayoutMode.RIGHT;
    }

    return (vy < 0) ? LayoutMode.TOP : LayoutMode.BOTTOM;
  }

  get orientation() {
    return (this.dir.y === 0) ? ORIENTATION.HORIZ : ORIENTATION.VERT;
  }

  get cursor() {
    return (this.dir.y === 0) ? 'col-resize' : 'row-resize';
  }

  // clang-format off
  static LEFT   = new LayoutMode('left',   new PointXY(-1,  0));
  static RIGHT  = new LayoutMode('right',  new PointXY( 1,  0));
  static TOP    = new LayoutMode('top',    new PointXY( 0, -1));
  static BOTTOM = new LayoutMode('bottom', new PointXY( 0,  1));
  // clang-format on
}

/******** VisOptions ********/
/**
 * Defines globally shared, mutable state for rendering options, including
 * scale.
 */
class VisOptions {
  constructor() {
    this.densityFactor = 1.0;
    this.layoutMode = LayoutMode.LEFT;

    // The usage of scaling leads two to sets of dimensions / coordinates:
    // * "World": Prefixed by "w-", these represent actual pixels on the device,
    //   and is unaffected by zoom value `scale`.
    // * "Scaled": Prefixed by "s-", these represent UI pixels in the
    //   Screenshot, and is proportional to `scale` (from Zoom feature).

    /** @type {number} Index into `ZOOM_LEVELS`. */
    this.zoomIndex = ZOOM_FIT_INDEX;
    /** @type {number} Current ratio from World to Scaled. */
    this.scale = 1.0;
    /** @type {!Dims2D} World device dimensions. */
    this.wDims = new Dims2D(0, 0);
    /** @type {!Dims2D} Scaled device dimensions. */
    this.sDims = new Dims2D(0, 0);
  }

  setWorldSize(ww, wh) {
    this.wDims.assign(ww, wh);
  }

  setDensityFactor(factor) {
    this.densityFactor = factor > 0 ? factor : 1.0;
  }

  setZoomIndex(index) {
    this.zoomIndex = index;
  }

  isZoomFit() {
    return this.zoomIndex === ZOOM_FIT_INDEX;
  }

  getAutoResizeDims() {
    const targetScale = this.isZoomFit() ? 1.0 : this.scale;
    return this.wDims.clone().mulBy(targetScale);
  }

  /**
   * Recalculates the visual scale based on viewport constraints.
   * @param {!Dims2D} vpDims The browser viewport dimensions to show Screenshot.
   */
  updateGeometry(vpDims) {
    if (this.wDims.w <= 0 || this.wDims.h <= 0) {
      this.scale = 1.0;
      this.sDims.assign(0, 0);
      return;
    }

    if (this.isZoomFit()) {
      if (this.layoutMode.orientation === ORIENTATION.HORIZ) {
        // Constrained by width. Due to browser rounding, `vpDims.w` may drift
        // away from integer, e.g., 799.984375. For better fidelity, we use
        // `round()` (not `floor()`) and restore this to, e.g., 800px. Note that
        // Screenshot result may be fractionally larger than the viewport!
        // Fortunately, this won't create a 1px-shift scrollbar to appear, since
        // apparently the decision to show scrollbar also uses pre-round sizes.
        const sw = Math.round(vpDims.w);
        this.scale = sw / this.wDims.w;
        this.sDims.assign(sw, this.wDims.h * this.scale);
      } else {
        // Constrained by height. Similar consideration as the above.
        const sh = Math.round(vpDims.h);
        this.scale = sh / this.wDims.h;
        this.sDims.assign(this.wDims.w * this.scale, sh);
      }
    } else {
      this.scale = ZOOM_LEVELS[this.zoomIndex].scale;
      this.sDims.assign(this.wDims.w * this.scale, this.wDims.h * this.scale);
    }
  }

  pxToDp(px) {
    return px / this.densityFactor;
  }
}

/******** ViewNode ********/
/**
 * A single Android View parsed from the UI Dump, as a node in the View Tree.
 */
class ViewNode {
  /**
   * @param {number} index
   * @param {?ViewNode} parent
   * @param {number} depth
   * @param {boolean} isLastChild
   * @param {string} className
   * @param {string} resourceId
   * @param {!Rectangle} rect
   */
  constructor(index, parent, depth, isLastChild, className, resourceId, rect) {
    this.index = index;
    this.parent = parent;
    this.depth = depth;
    this.isLastChild = isLastChild;
    this.children = [];

    this.className = className;
    this.resourceId = resourceId;

    this.rect = rect;
  }

  /**
   * Parses Android XML bounds string "[x1,y1][x2,y2]".
   * @param {?string} rectStr
   * @return {!Rectangle} The parsed rectangle, or a 0x0 rectangle on error.
   */
  static parseRect(rectStr) {
    if (rectStr) {
      const match = rectStr.match(/-?\d+/g);
      if (match && match.length >= 4) {
        return Rectangle.fromComponents(...match.map(Number));
      }
    }
    console.warn(`Bad rectangle: ${rectStr}`);
    return Rectangle.fromComponents(0, 0, 0, 0);
  }

  /**
   * Factory method to instantiate a {@link ViewNode} from an Android UI Dump.
   * XML Element.
   * @param {!Element} xmlNode
   * @param {number} index
   * @param {?ViewNode} parent
   * @param {number} depth
   * @param {boolean} isLastChild
   * @return {!ViewNode}
   */
  static fromXml(xmlNode, index, parent, depth, isLastChild) {
    const className = xmlNode.getAttribute('class') || '';
    const resourceId = xmlNode.getAttribute('resource-id') || '';
    const rect = ViewNode.parseRect(xmlNode.getAttribute('bounds'));
    return new ViewNode(index, parent, depth, isLastChild, className,
                        resourceId, rect);
  }
}

/******** ViewEngaged ********/
class ViewEngaged {
  constructor() {
    /** @type {?number} The active hover index, null for none. */
    this.hover = null;
  }

  clear() {
    this.hover = null;
  }
}

/******** MainModel ********/
/**
 * Global source of truth representing the device state. Owns data fetching
 * from the ADB server, XML parsing, and the View hierarchy model.
 */
class MainModel {
  constructor() {
    this.visOpts = new VisOptions();
    this.isLoaded = false;
    this.imgScreenshot = null;
    this.views = [];
    this.engaged = new ViewEngaged();
  }

  /**
   * Visits parsed Android UI dump XML hierarchy, instantiates {@link ViewNode}
   * from its Element, and writes them into {@link MainModel#views}.
   * @param {!Document} xmlDoc Parsed Android UI dump XML.
   */
  _populateViews(xmlDoc) {
    this.views.length = 0;
    const xmlRoot = xmlDoc.querySelector('hierarchy');
    if (!xmlRoot) return;

    const makeFrame = (viewParent, xmlNode) => ({
      viewParent,
      xmlChildrenElements:
          Array.from(xmlNode.children).filter(n => n.nodeType === 1),
      i: 0
    });

    // Recursion-free pre-order DFS traversal of `xmlRoot`'s Element children.
    const stack = [makeFrame(null, xmlRoot)];
    while (stack.length > 0) {
      const fr = stack.at(-1);
      if (fr.i < fr.xmlChildrenElements.length) {
        const xmlChild = fr.xmlChildrenElements[fr.i++];
        const depth = stack.length - 1;
        const isLastChild = (fr.i === fr.xmlChildrenElements.length);
        const view = ViewNode.fromXml(xmlChild, this.views.length,
                                      fr.viewParent, depth, isLastChild);
        if (fr.viewParent) fr.viewParent.children.push(view);
        this.views.push(view);
        stack.push(makeFrame(view, xmlChild));
      } else {
        stack.pop();
      }
    }
  }

  /** Fetches the latest device data from the ADB server. */
  async _fetchData() {
    const [screenshotResponse, uiDumpResponse, densityResponse] =
        await Promise.all([
          fetch('/api/screenshot.png'), fetch('/api/ui-dump.xml'),
          fetch('/api/density.json')
        ]);

    if (!screenshotResponse.ok) throw new Error('Screenshot fetch failed');
    if (!uiDumpResponse.ok) throw new Error('UI dump fetch failed');

    const screenshotBlob = await screenshotResponse.blob();

    const uiDumpData = await uiDumpResponse.json();
    if (!uiDumpData.success) throw new Error(uiDumpData.error);

    // Make density data optional, and fall back to 1.0.
    let densityData = null;
    try {
      if (!densityResponse.ok) throw new Error('Density fetch failed');

      densityData = await densityResponse.json();
      if (!densityData.success) throw new Error(densityData.error);
    } catch (e) {
      console.error(e);
      console.error('Fall back to density factor of 1.0');
      densityData = {density_factor: 1.0};
    }

    return {screenshotBlob, uiDumpData, densityData};
  }

  async load() {
    const {screenshotBlob, uiDumpData, densityData} = await this._fetchData();

    this.imgScreenshot =
        await convertImageBlobToImage(screenshotBlob).catch((e) => {
          throw new Error('Failed to convert screenshot image');
        });
    const {width, height} = this.imgScreenshot;
    this.visOpts.setWorldSize(width, height);

    this.visOpts.setDensityFactor(densityData.density_factor);

    const xmlDoc = new DOMParser().parseFromString(uiDumpData.xml, 'text/xml');
    this._populateViews(xmlDoc);

    this.isLoaded = true;
  }

  async unload() {
    this.isLoaded = false;

    this.engaged.clear();
    this.views.length = 0;
    this.imgScreenshot = null;

    // Reset geometry constraints.
    this.visOpts.sDims.assign(0, 0);
    this.visOpts.scale = 1.0;
  }
}
