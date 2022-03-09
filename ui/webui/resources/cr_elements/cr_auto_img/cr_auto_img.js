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
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png">
 *
 *      If your image needs to be fetched using cookies, you can use the
 *      with-cookies attribute as follows:
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png" with-cookies>
 *
 *      If you want the image to reset to an empty state when auto-src changes
 *      and the new image is still loading, set the clear-src attribute:
 *
 *      <img is="cr-auto-img" auto-src="[[calculateSrc()]]" clear-src>
 *
 * NOTE: Since <cr-auto-img> may use the chrome://image data source some images
 * may be transcoded to PNG.
 */

/** @type {string} */
const AUTO_SRC = 'auto-src';

/** @type {string} */
const CLEAR_SRC = 'clear-src';

/** @type {string} */
const WITH_COOKIES = 'with-cookies';

export class CrAutoImgElement extends HTMLImageElement {
  static get observedAttributes() {
    return [AUTO_SRC, WITH_COOKIES];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name !== AUTO_SRC && name !== WITH_COOKIES) {
      return;
    }

    // Changes to |WITH_COOKIES| are only interesting when the attribute is
    // being added or removed.
    if (name === WITH_COOKIES &&
        ((oldValue === null) === (newValue === null))) {
      return;
    }

    if (this.hasAttribute(CLEAR_SRC)) {
      // Remove the src attribute so that the old image is not shown while the
      // new one is loading.
      this.removeAttribute('src');
    }

    let url = null;
    try {
      url = new URL(this.getAttribute(AUTO_SRC) || '');
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
  set clearSrc(_) {
    this.setAttribute(CLEAR_SRC, '');
  }

  /** @return {string} */
  get clearSrc() {
    return this.getAttribute(CLEAR_SRC);
  }

  /** @param {boolean} enabled */
  set withCookies(enabled) {
    if (enabled) {
      this.setAttribute(WITH_COOKIES, '');
    } else {
      this.removeAttribute(WITH_COOKIES);
    }
  }

  /** @return {boolean} */
  get withCookies() {
    return this.hasAttribute(WITH_COOKIES);
  }
}

customElements.define('cr-auto-img', CrAutoImgElement, {extends: 'img'});
