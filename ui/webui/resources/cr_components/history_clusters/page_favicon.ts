// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getFaviconForPageURL} from '../../js/icon.js';
import {loadTimeData} from '../../js/load_time_data.js';

import {getTemplate} from './page_favicon.html.js';

/**
 * @fileoverview This file provides a custom element displaying a page favicon.
 */

declare global {
  interface HTMLElementTagNameMap {
    'page-favicon': PageFavicon;
  }
}

/**
 * TODO(tommycli): This element should be renamed to reflect the reality that
 * it's used to both render the visit's "important image" if it exists, and
 * falls back to the favicon if it doesn't exist.
 */
class PageFavicon extends PolymerElement {
  static get is() {
    return 'page-favicon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('inSidePanel'),
        reflectToAttribute: true,
      },

      /**
       * The element's style attribute.
       */
      style: {
        type: String,
        computed: `computeStyle_(url, imageUrl)`,
        reflectToAttribute: true,
      },

      /**
       * The URL for which the favicon is shown.
       */
      url: Object,

      /**
       * The URL of the representative image for the page. Not every page has
       * this defined, in which case we fallback to the favicon.
       */
      imageUrl: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  url: Url;
  imageUrl: Url;

  //============================================================================
  // Helper methods
  //============================================================================

  private computeStyle_(): string {
    if (this.imageUrl && this.imageUrl.url) {
      // Pages with a pre-set image URL don't show the favicon.
      return '';
    }

    if (!this.url) {
      return '';
    }
    return `background-image:${
        getFaviconForPageURL(
            this.url.url, false, '', /** --favicon-size */ 16)}`;
  }
}

customElements.define(PageFavicon.is, PageFavicon);
