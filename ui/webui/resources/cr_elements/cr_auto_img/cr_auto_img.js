// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview <cr-auto-img> is a specialized <img> that facilitates embedding
 * images into WebUIs via its auto-src attribute. <cr-auto-img> automatically
 * determines if the image is local (e.g. data: or chrome://) or external (e.g.
 * https://), and embeds the image directly or via the chrome://image data
 * source accordingly. Usage:
 *
 *   1. In C++ register |SanitizedImageSource| for your WebUI.
 *
 *   2. In HTML instantiate
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png"></img>
 *
 *      If your image needs to be fetched using cookies, you can use the
 *      with-cookies attribute as follows:
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png" with-cookies>
 *      </img>
 *
 * NOTE: Since <cr-auto-img> may use the chrome://image data source some images
 * may be transcoded to PNG.
 */

/** @type {string} */
const AUTO_SRC = 'auto-src';

/** @type {string} */
const WITH_COOKIES = 'with-cookies';

export class CrAutoImgElement extends HTMLImageElement {
  static get observedAttributes() {
    return [AUTO_SRC, WITH_COOKIES];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name !== AUTO_SRC) {
      return;
    }

    let url = null;
    try {
      url = new URL(newValue || '');
    } catch (_) {
    }

    if (!url || url.protocol === 'chrome-untrusted:') {
      // Loading chrome-untrusted:// directly kills the renderer process.
      // Loading chrome-untrusted:// via the chrome://image data source
      // results in a broken image.
      this.removeAttribute('src');
    } else if (url.protocol === 'data:' || url.protocol === 'chrome:') {
      this.src = url.href;
    } else if (this.hasAttribute(WITH_COOKIES)) {
      this.src =
          `chrome://image?url=${encodeURIComponent(url.href)}&withCookies=true`;
    } else {
      this.src = 'chrome://image?' + url.href;
    }
  }

  /** @param {string} src */
  set autoSrc(src) {
    this.setAttribute(AUTO_SRC, src);
  }

  /** @return {string} */
  get autoSrc() {
    return this.getAttribute(AUTO_SRC);
  }

  /** @param {string} _ */
  set withCookies(_) {
    this.setAttribute(WITH_COOKIES, '');
  }

  /** @return {string} */
  get withCookies() {
    return this.getAttribute(WITH_COOKIES);
  }
}

customElements.define('cr-auto-img', CrAutoImgElement, {extends: 'img'});
