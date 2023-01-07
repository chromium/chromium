// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds the functions that allow WebUI to update its
 * colors CSS stylesheet when a ColorProvider change in the browser is detected.
 */

import {BrowserProxy} from './browser_proxy.js';

/**
 * The CSS selector used to get the <link> node with the colors.css stylesheet.
 * The wildcard is needed since the URL ends with a timestamp.
 */
export const COLORS_CSS_SELECTOR: string = 'link[href*=\'colors.css\']';

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
  const params = new URLSearchParams(hrefURL.search);
  params.set('version', new Date().getTime().toString());
  const newHref = `${hrefURL.origin}${hrefURL.pathname}?${params.toString()}`;

  // A flickering effect may take place when setting the href property of the
  // existing color css node with a new value. In order to avoid flickering, we
  // create a new link element and once it is loaded we remove the old one. See
  // crbug.com/1365320 for additional details.
  const newColorsCssLink = document.createElement('link');
  newColorsCssLink.setAttribute('href', newHref);
  newColorsCssLink.rel = 'stylesheet';
  newColorsCssLink.type = 'text/css';
  newColorsCssLink.onload = (() => {
    const oldColorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
    if (oldColorCssNode) {
      oldColorCssNode.remove();
    }
  });
  document.getElementsByTagName('body')[0]!.appendChild(newColorsCssLink);

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
