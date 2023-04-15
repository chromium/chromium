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
export const COLORS_CSS_SELECTOR: string = 'link[href*=\'//theme/colors.css\']';

/**
 * Forces the document to refresh its colors.css stylesheet. This is used to
 * fetch an updated stylesheet when the ColorProvider associated with the WebUI
 * has changed.
 * Returns a promise which resolves to true once the new colors are loaded and
 * installed into the DOM. In the case of an error returns false.
 */
export async function refreshColorCss(): Promise<boolean> {
  const colorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
  if (!colorCssNode) {
    return false;
  }
  const href = colorCssNode.getAttribute('href');
  if (!href) {
    return false;
  }

  const hrefURL = new URL(href, location.href);
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
  const newColorsLoaded = new Promise(resolve => {
    newColorsCssLink.onload = resolve;
  });
  document.getElementsByTagName('body')[0]!.appendChild(newColorsCssLink);

  await newColorsLoaded;

  const oldColorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
  if (oldColorCssNode) {
    oldColorCssNode.remove();
  }
  return true;
}



let listenerId: number|null = null;
let clientColorChangeListeners: Array<() => void> = [];

/**
 * Calls `refreshColorCss()` and any listeners previously registered via
 * `addColorChangeListener()`
 */
export async function colorProviderChangeHandler() {
  // The webui's current css variables may now be stale, force update them.
  await refreshColorCss();
  // Notify any interested javascript that the color scheme has changed.
  for (const listener of clientColorChangeListeners) {
    listener();
  }
}

/**
 * Register a function to be called every time the page's color provider
 * changes. Note that the listeners will only be invoked AFTER
 * startColorChangeUpdater() is called.
 */
export function addColorChangeListener(changeListener: () => void) {
  clientColorChangeListeners.push(changeListener);
}

/**
 * Remove a listener that was previously registered via addColorChangeListener.
 * If provided with a listener that was not previously registered does nothing.
 */
export function removeColorChangeListener(changeListener: () => void) {
  clientColorChangeListeners = clientColorChangeListeners.filter(
      listener => listener !== changeListener);
}

/** Starts listening for ColorProvider change updates from the browser. */
export function startColorChangeUpdater() {
  if (listenerId === null) {
    listenerId = BrowserProxy.getInstance()
                     .callbackRouter.onColorProviderChanged.addListener(
                         colorProviderChangeHandler);
  }
}
