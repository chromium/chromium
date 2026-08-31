// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** ScreenshotVis ********/
/**
 * Renders the device's image into an HTML canvas.
 */
class ScreenshotVis {
  constructor(divScreenshot, visOpts) {
    this.el = {
      inner: divScreenshot.querySelector('.screenshot-inner'),
      canvBase: divScreenshot.querySelector('.canv-base'),
    };
    this.visOpts = visOpts;

    this.ctx = this.el.canvBase.getContext('2d');
  }

  /**
   * @param {!Image} imgScreenshot
   * @param {!VisOptions} visOpts
   */
  init(imgScreenshot, visOpts) {
    const el = this.el;
    const {width, height} = imgScreenshot;
    el.canvBase.width = width;
    el.canvBase.height = height;
    this.ctx.drawImage(imgScreenshot, 0, 0);
  }

  clear() {
    this.ctx.clearRect(0, 0, this.el.canvBase.width, this.el.canvBase.height);
  }
}
