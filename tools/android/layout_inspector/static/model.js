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

/******** MainModel ********/
/**
 * Global source of truth representing the device state. Owns data fetching from
 * the ADB server.
 */
class MainModel {
  constructor() {
    this.visOpts = new VisOptions();
    this.imgScreenshot = null;
  }

  /** Fetches the latest device data from the ADB server. */
  async _fetchData() {
    const screenshotResponse = await fetch('/api/screenshot.png');

    if (!screenshotResponse.ok) throw new Error('Screenshot fetch failed');

    const screenshotBlob = await screenshotResponse.blob();

    return {screenshotBlob};
  }

  async load() {
    const {screenshotBlob} = await this._fetchData();

    this.imgScreenshot =
        await convertImageBlobToImage(screenshotBlob).catch((e) => {
          throw new Error('Failed to convert screenshot image');
        });
    const {width, height} = this.imgScreenshot;
    this.visOpts.setWorldSize(width, height);
  }

  async unload() {
    this.imgScreenshot = null;
  }
}
