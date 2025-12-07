// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './searchbox_thumbnail.css.js';
import {getHtml} from './searchbox_thumbnail.html.js';

// Displays a thumbnail in the searchbox input.
export class SearchboxThumbnailElement extends CrLitElement {
  static get is() {
    return 'cr-searchbox-thumbnail';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Private properties
      //========================================================================
      thumbnailUrl_: {type: String},

      isDeletable_: {
        type: Boolean,
        reflect: true,
      },

      enableThumbnailSizingTweaks_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // The URL of the thumbnail to display.
  protected accessor thumbnailUrl_: string = '';
  // Whether the user can delete the thumbnail.
  protected accessor isDeletable_: boolean = false;
  // Whether to enable thumbnail sizing tweaks.
  protected accessor enableThumbnailSizingTweaks_: boolean =
      loadTimeData.getBoolean('enableThumbnailSizingTweaks');

  //============================================================================
  // Event handlers
  //============================================================================

  protected onRemoveButtonClick_(e: Event) {
    e.preventDefault();
    this.fire('remove-thumbnail-click');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-thumbnail': SearchboxThumbnailElement;
  }
}

customElements.define(SearchboxThumbnailElement.is, SearchboxThumbnailElement);
