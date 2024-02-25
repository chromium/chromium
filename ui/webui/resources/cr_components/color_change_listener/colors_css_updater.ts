// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds the functions that allow WebUI to update its
 * colors CSS stylesheet when a ColorProvider change in the browser is detected.
 */

import {assert} from '//resources/js/assert.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * The CSS selector used to get the <link> node with the colors.css stylesheet.
 * The wildcard is needed since the URL ends with a timestamp.
 */
export const COLORS_CSS_SELECTOR: string = 'link[href*=\'//theme/colors.css\']';

let documentInstance: ColorChangeUpdater|null = null;

// <if expr="chromeos_ash">
// Event fired after updated colors have been fetched and applied.
export const COLOR_PROVIDER_CHANGED: string = 'color-provider-changed';
// </if>

export class ColorChangeUpdater {
  private listenerId_: null|number = null;
  private root_: Document|ShadowRoot;

  // <if expr="chromeos_ash">
  eventTarget: EventTarget = new EventTarget();
  // </if>

  constructor(root: Document|ShadowRoot) {
    assert(documentInstance === null || root !== document);
    this.root_ = root;
  }

  /**
   * Starts listening for ColorProvider changes from the browser and updates the
   * `root_` whenever changes occur.
   */
  start() {
    if (this.listenerId_ !== null) {
      return;
    }

    this.listenerId_ = BrowserProxy.getInstance()
                           .callbackRouter.onColorProviderChanged.addListener(
                               this.onColorProviderChanged.bind(this));
  }

  // TODO(dpapad): Figure out how to properly trigger
  // `callbackRouter.onColorProviderChanged` listeners from tests and make this
  // method private.
  async onColorProviderChanged() {
    await this.refreshColorsCss();
    // <if expr="chromeos_ash">
    this.eventTarget.dispatchEvent(new CustomEvent(COLOR_PROVIDER_CHANGED));
    // </if>
  }

  /**
   * Forces `root_` to refresh its colors.css stylesheet. This is used to
   * fetch an updated stylesheet when the ColorProvider associated with the
   * WebUI has changed.
   * @return A promise which resolves to true once the new colors are loaded and
   *     installed into the DOM. In the case of an error returns false. When a
   *     new colors.css is loaded, this will always freshly query the existing
   *     colors.css, allowing multiple calls to successfully remove existing,
   *     outdated CSS.
   */
  async refreshColorsCss(): Promise<boolean> {
    const colorCssNode = this.root_.querySelector(COLORS_CSS_SELECTOR);
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

    // A flickering effect may take place when setting the href property of
    // the existing color css node with a new value. In order to avoid
    // flickering, we create a new link element and once it is loaded we
    // remove the old one. See crbug.com/1365320 for additional details.
    const newColorsCssLink = document.createElement('link');
    newColorsCssLink.setAttribute('href', newHref);
    newColorsCssLink.rel = 'stylesheet';
    newColorsCssLink.type = 'text/css';
    const newColorsLoaded = new Promise(resolve => {
      newColorsCssLink.onload = resolve;
    });
    if (this.root_ === document) {
      document.getElementsByTagName('body')[0]!.appendChild(newColorsCssLink);
    } else {
      this.root_.appendChild(newColorsCssLink);
    }

    await newColorsLoaded;

    const oldColorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
    if (oldColorCssNode) {
      oldColorCssNode.remove();
    }
    return true;
  }

  static forDocument(): ColorChangeUpdater {
    return documentInstance ||
        (documentInstance = new ColorChangeUpdater(document));
  }
}
