// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './segmented_button.html.js';

export class SegmentedButtonElement extends PolymerElement {
  static get is() {
    return 'segmented-button';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selected: {
        type: String,
        notify: true,
      },

      selectableElements: {
        type: String,
        value: 'segmented-button-option',
      },

      groupAriaLabel: String,
    };
  }

  selected: string;
  selectableElements: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'segmented-button': SegmentedButtonElement;
  }
}

customElements.define(SegmentedButtonElement.is, SegmentedButtonElement);
