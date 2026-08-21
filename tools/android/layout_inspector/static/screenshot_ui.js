// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** ScreenshotVis ********/
/**
 * Renders the device's image into an HTML canvas.
 */
class ScreenshotVis {
  constructor(divScreenshot, divPointerInfo, visOpts) {
    this.el = {
      inner: divScreenshot.querySelector('.screenshot-inner'),
      canvBase: divScreenshot.querySelector('.canv-base'),
    };
    this.visOpts = visOpts;

    this.ctx = this.el.canvBase.getContext('2d');
  }

  drawScreenshot(imageBlob, visOpts) {
    const el = this.el;
    const imageUrl = URL.createObjectURL(imageBlob);
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => {
        el.inner.classList.add('active');
        el.canvBase.width = img.width;
        el.canvBase.height = img.height;
        this.ctx.drawImage(img, 0, 0);
        URL.revokeObjectURL(imageUrl);
        resolve();
      };
      img.onerror = (e) => {
        URL.revokeObjectURL(imageUrl);
        reject(new Error('Failed to load screenshot image'));
      };
      img.src = imageUrl;
    });
  }

  clear() {
    this.el.inner.classList.remove('active');
    this.ctx.clearRect(0, 0, this.el.canvBase.width, this.el.canvBase.height);
  }
}
