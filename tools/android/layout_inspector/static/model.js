// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Constants ********/

/**
 * Units for displaying lengths in markup labels.
 * @enum {string}
 */
const LENGTH_UNIT = {
  DP: 'dp',
  PX: 'px',
};

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

  /** @return {ORIENTATION} */
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
    /** @type {LENGTH_UNIT} */
    this.lengthUnit = LENGTH_UNIT.DP;
    /** @type {!LayoutMode} */
    this.layoutMode = LayoutMode.LEFT;

    // The usage of scaling leads to two sets of dimensions / coordinates:
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

  /** @return {number} World to Scaled coordinate (e.g. for lengths). */
  w2s(w) {
    return w * this.scale;
  }

  /** @return {number} World to integer Scaled coordinate. */
  w2sFloor(w) {
    return Math.floor(w * this.scale);
  }

  /**
   * Scaled to World coordinate, given physical pixels taken from UI input.
   * Integer points in World coordinates are grid lines, but integer points
   * from UI are centers of pixels; this adds a 0.5 offset to align with the
   * pixel center domain.
   * @param {number} s
   * @return {number}
   */
  s2wDevice(s) {
    return (this.scale > 0) ? (s + 0.5) / this.scale : 0;
  }

  /**
   * Formats a length in device pixels using the active {@link lengthUnit}.
   * @param {number} px
   * @return {string}
   */
  formatLength(px) {
    if (this.lengthUnit === LENGTH_UNIT.PX) {
      return `${Math.round(px)}`;
    }
    const dp = this.pxToDp(px);
    return `${Math.round(dp)}`;
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
   * @param {string} className
   * @param {string} resourceId
   * @param {!Rectangle} rect
   */
  constructor(index, parent, depth, className, resourceId, rect) {
    this.index = index;
    this.parent = parent;
    this.depth = depth;
    this.children = [];

    this.className = className;
    this.resourceId = resourceId;

    this.rect = rect;
    this.area = this.rect.area;
  }

  /**
   * Factory method to instantiate a {@link ViewNode} from a <window> tag.
   * @param {!Element} xmlWindow
   * @param {number} index
   * @return {!{view: !ViewNode, initiallyExpanded: boolean}}
   */
  static fromXmlWindow(xmlWindow, index) {
    const type = xmlWindow.getAttribute('type') || 'Window';
    const title = xmlWindow.getAttribute('title');
    const rect = Rectangle.fromAndroidBounds(xmlWindow.getAttribute('bounds'));
    const view = new ViewNode(index, null, 0, type, title || type, rect);
    const initiallyExpanded =
        (type !== 'TYPE_SYSTEM' && type !== 'TYPE_SPLIT_SCREEN_DIVIDER');
    return {view, initiallyExpanded};
  }

  /**
   * Factory method to instantiate a {@link ViewNode} from a <node> tag.
   * @param {!Element} xmlNode
   * @param {number} index
   * @param {?ViewNode} parent
   * @param {number} depth
   * @return {!ViewNode}
   */
  static fromXmlNode(xmlNode, index, parent, depth) {
    const className = xmlNode.getAttribute('class') || '';
    const resourceId = xmlNode.getAttribute('resource-id') || '';
    const rect = Rectangle.fromAndroidBounds(xmlNode.getAttribute('bounds'));
    return new ViewNode(index, parent, depth, className, resourceId, rect);
  }
}

/******** HitTester ********/
/**
 * Grid-based spatial partition to accelerate view lookups.
 */
class HitTester {
  /**
   * @param {!Array<!ViewNode>} views
   * @param {!Dims2D} wDims
   * @param {number=} cellSize
   */
  constructor(views, wDims, cellSize = 16) {
    // To optimize hit test, the screen is partitioned into square cells (each
    // measuring `cellSize` x `cellSize`), forming a lower-resolution grid. Each
    // cell stores a list of `ViewNode`s that intersect with it. The data is
    // initialized here by "painting" each `view` into the grid. `query()` will
    // then perform hit test for a given point `(x, y)` as follows: Find the
    // containing cell, visit the stored `ViewNode`s, check `(x, y)` inclusion,
    // and select the best candidate.

    /** @type {number} Width and height of each cell in grid. */
    this.cellSize = cellSize;

    /** @type {number} Grid width, not using `Dims2D` due to frequent access. */
    this.gridW = Math.ceil(wDims.w / cellSize);
    /** @type {number} Grid height. */
    this.gridH = Math.ceil(wDims.h / cellSize);

    /**
     * @type {!Array<?Array<!ViewNode>>} Flattened 1D array that maps each cell
     *     to a list of `ViewNode`s that intersects with it.
     */
    this.grid = Array(this.gridW * this.gridH).fill(null);

    // A View's Rectangle coordinates are pixel *boundaries*, but hit tests use
    // pixel *interior*. Use `right - 1` and `bottom - 1` before floor-division
    // so inclusive-inclusive cell bounds don't spill into adjacent cells when
    // edges lie on cell boundaries.
    const maxX = this.gridW - 1;
    const maxY = this.gridH - 1;

    for (const view of views) {
      const {left, top, right, bottom} = view.rect;
      if (left >= right || top >= bottom || right <= 0 || left >= wDims.w ||
          bottom <= 0 || top >= wDims.h) {
        continue;
      }
      const c1 = clip(0, Math.floor(left / cellSize), maxX);
      const c2 = clip(0, Math.floor((right - 1) / cellSize), maxX);
      const r1 = clip(0, Math.floor(top / cellSize), maxY);
      const r2 = clip(0, Math.floor((bottom - 1) / cellSize), maxY);

      for (let row = r1; row <= r2; row++) {
        for (let col = c1; col <= c2; col++) {
          const index = row * this.gridW + col;
          if (!this.grid[index]) {
            this.grid[index] = [];
          }
          this.grid[index].push(view);
        }
      }
    }
  }

  /**
   * @param {!PointXY} wpt
   * @return {?ViewNode} The smallest / deepest View containing `wpt`, chosen so
   *     that large (possibly ancestral) Views would not obscure smaller ones.
   */
  query(wpt) {
    const c = Math.floor(wpt.x / this.cellSize);
    const r = Math.floor(wpt.y / this.cellSize);
    if (c < 0 || c >= this.gridW || r < 0 || r >= this.gridH) {
      return null;
    }

    const index = r * this.gridW + c;
    const candidates = this.grid[index] || [];
    let best = null;

    for (const view of candidates) {
      if (view.rect.containsInclExcl(wpt)) {
        // Tie-breaker: Later index (appearing later in view list).
        if (!best || (view.area < best.area) ||
            (view.area === best.area && view.index > best.index)) {
          best = view;
        }
      }
    }
    return best;
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

    /** @type {!Array<!{view: !ViewNode, initiallyExpanded: boolean}>} */
    this.windowInfos = [];
    /** @type {!Array<!ViewNode>} */
    this.views = [];

    this.engaged = new ViewEngaged();
    this.hitTester = null;
  }

  /**
   * Visits parsed Android UI dump XML hierarchy, instantiates {@link ViewNode}
   * from its Element, and writes them into {@link MainModel#views}.
   * @param {!Element} xmlHierarchy Parsed Android UI dump XML `<hierarchy>`.
   * @param {?ViewNode} ancestorView Optional ancestor {@link ViewNode} to
   *     attach tree to.
   */
  _populateViewTree(xmlHierarchy, ancestorView) {
    // Recursion-free pre-order DFS traversal of the Window's View hierarchy.
    const stack = [{parentView: ancestorView, parentXml: xmlHierarchy, i: 0}];
    while (stack.length > 0) {
      const frame = stack.at(-1);
      if (frame.i < frame.parentXml.children.length) {
        const childXml = frame.parentXml.children[frame.i++];
        const depth = frame.parentView ? frame.parentView.depth + 1 : 0;
        const view = ViewNode.fromXmlNode(childXml, this.views.length,
                                          frame.parentView, depth);
        if (frame.parentView) frame.parentView.children.push(view);
        this.views.push(view);
        stack.push({parentView: view, parentXml: childXml, i: 0});
      } else {
        stack.pop();
      }
    }
  }

  /**
   * Visits parsed Android UI dump XML hierarchy, instantiates {@link ViewNode}
   * from its Element, and writes them into {@link MainModel#views}.
   * @param {!Document} xmlDoc Parsed Android UI dump XML.
   */
  _populateViewForest(xmlDoc) {
    this.views.length = 0;
    this.windowInfos.length = 0;

    const xmlWindows = xmlDoc.querySelectorAll('window');
    if (xmlWindows.length > 0) {
      for (const xmlWindow of xmlWindows) {
        const windowInfo = ViewNode.fromXmlWindow(xmlWindow, this.views.length);
        this.windowInfos.push(windowInfo);
        const windowView = windowInfo.view;
        this.views.push(windowView);
        const xmlHierarchy = xmlWindow.querySelector(':scope > hierarchy');
        if (xmlHierarchy) this._populateViewTree(xmlHierarchy, windowView);
      }
    } else {  // Backwards compatibility.
      const xmlHierarchy = xmlDoc.querySelector('hierarchy');
      if (xmlHierarchy) this._populateViewTree(xmlHierarchy, null);
    }
  }

  /**
   * @param {!PointXY} wpt
   * @return {?number} Index of the hit View, or null if none.
   */
  queryHit(wpt) {
    return this.hitTester ? (this.hitTester.query(wpt)?.index ?? null) : null;
  }

  /** Fetches the latest device data from the ADB server. */
  async _fetchData() {
    const [screenshotResponse, uiDumpResponse, densityResponse] =
        await Promise.all([
          fetch('/api/screenshot.png'), fetch('/api/ui-dump.json'),
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
    this._populateViewForest(xmlDoc);
    this.hitTester = new HitTester(this.views, this.visOpts.wDims);

    this.isLoaded = true;
  }

  async unload() {
    this.isLoaded = false;

    this.engaged.clear();
    this.hitTester = null;
    this.views.length = 0;
    this.windowInfos.length = 0;
    this.imgScreenshot = null;

    // Reset geometry constraints.
    this.visOpts.sDims.assign(0, 0);
    this.visOpts.scale = 1.0;
  }
}
