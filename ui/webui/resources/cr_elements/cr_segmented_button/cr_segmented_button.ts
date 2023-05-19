// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_radio_group/cr_radio_group.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_segmented_button.html.js';

export class CrSegmentedButtonElement extends PolymerElement {
  static get is() {
    return 'cr-segmented-button';
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
        value: 'cr-segmented-button-option',
      },
    };
  }

  public selected: string;
  public selectableElements: string;
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-segmented-button': CrSegmentedButtonElement;
  }
}

customElements.define(CrSegmentedButtonElement.is, CrSegmentedButtonElement);
