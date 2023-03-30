// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
        computed: `computeStyle_(url, imageUrl_)`,
        reflectToAttribute: true,
      },

      /**
       * The URL for which the favicon is shown.
       */
      url: Object,

      /**
       * Whether this visit is known to sync already. Used for the purpose of
       * fetching higher quality favicons in that case.
       */
      isKnownToSync: Boolean,

      /**
       * The URL of the representative image for the page. Not every page has
       * this defined, in which case we fallback to the favicon.
       */
      imageUrl_: {
        type: Object,
        value: null,
      },

      isImageCover_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isHistoryClustersImageCover'),
        reflectToAttribute: true,
      },
    };
  }

  static get observers() {
    return ['urlAndIsKnownToSyncChanged_(url, isKnownToSync)'];
  }

  //============================================================================
  // Properties
  //============================================================================

  url: Url;
  isKnownToSync: boolean;
  private imageUrl_: Url|null;

  //============================================================================
  // Helper methods
  //============================================================================

  getImageUrlForTesting(): Url|null {
    return this.imageUrl_;
  }

  private computeStyle_(): string {
    if (this.imageUrl_ && this.imageUrl_.url) {
      // Pages with a pre-set image URL don't show the favicon.
      return '';
    }

    if (!this.url) {
      return '';
    }
    return `background-image:${
        getFaviconForPageURL(
            this.url.url, this.isKnownToSync, '', /** --favicon-size */ 16)}`;
  }

  private async urlAndIsKnownToSyncChanged_() {
    if (!this.url || !this.isKnownToSync ||
        !loadTimeData.getBoolean('isHistoryClustersImagesEnabled')) {
      this.imageUrl_ = null;
      return;
    }

    // Fetch the representative image for this page, if possible.
    const {result} =
        await PageImageServiceBrowserProxy.getInstance()
            .handler.getPageImageUrl(
                PageImageServiceClientId.Journeys, this.url,
                {suggestImages: true, optimizationGuideImages: true});
    if (result) {
      this.imageUrl_ = result.imageUrl;
    } else {
      // We must reset imageUrl_ to null, because sometimes the Virtual DOM will
      // reuse the same element for the infinite scrolling list.
      this.imageUrl_ = null;
    }
  }
}

customElements.define(PageFavicon.is, PageFavicon);
