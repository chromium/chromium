// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './realbox_thumbnail.html.js';

const ThumbnailElementBase = I18nMixin(PolymerElement);

// Displays a thumbnail in the realbox input.
class RealboxThumbnailElement extends ThumbnailElementBase {
  static get is() {
    return 'cr-realbox-thumbnail';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      //========================================================================
      // Private properties
      //========================================================================
      thumbnailUrl_: {
        type: String,
      },

    };
  }

  private thumbnailUrl_: string;

  //============================================================================
  // Event handlers
  //============================================================================

  private onRemoveButtonClick_(e: Event) {
    this.dispatchEvent(new CustomEvent('remove-thumbnail-click', {
      bubbles: true,
      composed: true,
    }));
    e.preventDefault();
  }
}

customElements.define(RealboxThumbnailElement.is, RealboxThumbnailElement);
