// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {getFaviconForPageURL, getUrlForCss} from '//resources/js/icon.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './composebox_tab_favicon.css.js';

/**
 * @fileoverview This file provides a custom element displaying a tab favicon.
 */

export class TabFaviconElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-tab-favicon';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      /* The URL for which the favicon is shown. */
      url: {type: String},
      size: {type: Number},
      tabId: {type: Number},
    };
  }

  accessor url: string = '';
  accessor size: number = 16;
  accessor tabId: number|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('url') || changedProperties.has('size') ||
        changedProperties.has('tabId')) {
      if (!this.url) {
        this.style.setProperty('background-image', '');
      } else {
        this.style.setProperty(
            'background-image',
            getFaviconForPageURL(
                this.url, /*isKnownToSync=*/ false, /*fallbackUrl=*/ '',
                this.size, false, true));
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if ((changedProperties.has('url') || changedProperties.has('tabId')) &&
        this.url && this.tabId) {
      const capturedTabId = this.tabId;
      this.fire('wait-for-tab-load', {
        tabId: capturedTabId,
        onTabLoaded: (faviconDataUrl?: string) => {
          if (faviconDataUrl && this.url && this.tabId === capturedTabId) {
            this.style.setProperty(
                'background-image', getUrlForCss(faviconDataUrl));
          }
        },
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-tab-favicon': TabFaviconElement;
  }
}

customElements.define(TabFaviconElement.is, TabFaviconElement);
