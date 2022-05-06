// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds the functions that allow WebUI to update its
 * colors CSS stylesheet when a ColorProvider change in the browser is detected.
 */

import {BrowserProxy} from './browser_proxy.js';

/**
 * The CSS selector used to get the <link> node with the colors.css stylesheet.
 */
export const COLORS_CSS_SELECTOR: string = 'link[href$=\'colors.css\']';

/**
 * Forces the document to refresh its colors.css stylesheet. This is used to
 * fetch an updated stylesheet when the ColorProvider associated with the WebUI
 * has changed.
 */
export function refreshColorCss(): boolean {
  const colorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
  if (!colorCssNode) {
    return false;
  }
  const href = colorCssNode.getAttribute('href');
  if (!href) {
    return false;
  }
  const hrefURL = new URL(href);
  const params =
      new URLSearchParams([['version', new Date().getTime().toString()]]);
  const newHref = `${hrefURL.origin}${hrefURL.pathname}?${params.toString()}`;
  colorCssNode.setAttribute('href', newHref);
  return true;
}

let listenerId: number|null = null;

/**
 * Starts listening for ColorProvider change updates from the browser.
 */
export function startColorChangeUpdater() {
  if (listenerId === null) {
    listenerId =
        BrowserProxy.getInstance()
            .callbackRouter.onColorProviderChanged.addListener(refreshColorCss);
  }
}
