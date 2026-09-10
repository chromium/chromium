// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
    this.layoutMode = LayoutMode.LEFT;

    /** @type {!Dims2D} World device dimensions. */
    this.wDims = new Dims2D(0, 0);
  }

  setWorldSize(ww, wh) {
    this.wDims.assign(ww, wh);
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
    const [screenshotResponse, uiDumpResponse] = await Promise.all(
        [fetch('/api/screenshot.png'), fetch('/api/ui-dump.xml')]);

    if (!screenshotResponse.ok) throw new Error('Screenshot fetch failed');
    if (!uiDumpResponse.ok) throw new Error('UI dump fetch failed');

    const screenshotBlob = await screenshotResponse.blob();

    const uiDumpData = await uiDumpResponse.json();
    if (!uiDumpData.success) throw new Error(uiDumpData.error);

    return {screenshotBlob, uiDumpData};
  }

  async load() {
    const {screenshotBlob, uiDumpData} = await this._fetchData();

    this.imgScreenshot =
        await convertImageBlobToImage(screenshotBlob).catch((e) => {
          throw new Error('Failed to convert screenshot image');
        });
    const {width, height} = this.imgScreenshot;
    this.visOpts.setWorldSize(width, height);

    const xmlDoc = new DOMParser().parseFromString(uiDumpData.xml, 'text/xml');
    this._populateViews(xmlDoc);
  }

  async unload() {
    this.views.length = 0;
    this.imgScreenshot = null;
  }
}
