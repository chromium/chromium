// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

const ZOOM_LEVELS = [
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
const ZOOM_LEVEL_DEFAULT_INDEX = 6;  // For 100%.

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
 * Defines globally shared, mutable state for rendering options.
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
    this.zoomIndex = ZOOM_LEVEL_DEFAULT_INDEX;
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

  /**
   * Recalculates the visual scale based on viewport constraints.
   * @param {!Dims2D} vpDims The current available browser viewport.
   */
  updateGeometry(vpDims) {
    if (this.wDims.w <= 0 || this.wDims.h <= 0) {
      this.scale = 1.0;
      this.sDims.assign(0, 0);
      return;
    }

    this.scale = ZOOM_LEVELS[this.zoomIndex].scale;
    this.sDims.assign(this.wDims.w * this.scale, this.wDims.h * this.scale);
  }

  pxToDp(px) {
    return px / this.densityFactor;
  }
}

/******** ViewNode ********/
/**
 * A single Android View parsed from the UI Dump, as a node in the View tree.
 */
class ViewNode {
  constructor(xmlNode, index, parent, depth, isLastChild) {
    this.xmlNode = xmlNode;
    this.index = index;
    this.parent = parent;
    this.depth = depth;
    this.isLastChild = isLastChild;
    this.expanded = true;
    this.children = [];

    this.className = xmlNode.getAttribute('class');
    this.resourceId = xmlNode.getAttribute('resource-id');
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
        const view = new ViewNode(xmlChild, this.views.length, fr.viewParent,
                                  depth, isLastChild);
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

    this.views.length = 0;
    this.imgScreenshot = null;
  }
}
