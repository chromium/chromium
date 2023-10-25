// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {Button} from 'chrome://resources/cros_components/button/button.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {isCrosComponentsEnabled} from '../../common/js/flags.js';

const htmlTemplate = html`{__html_template__}`;

/**
 * A button used inside PanelItem with varying display characteristics.
 */
export class PanelButton extends HTMLElement {
  constructor() {
    super();
    this.createElement_();
  }

  /**
   * Creates a PanelButton.
   * @private
   */
  createElement_() {
    const fragment = htmlTemplate.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Registers this instance to listen to these attribute changes.
   */
  static get observedAttributes() {
    return [
      'data-category',
    ];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   */
  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue === newValue) {
      return;
    }
    /** @type {?Element} */
    const iconButton = this.shadowRoot?.querySelector('cr-icon-button') ?? null;
    if (name === 'data-category') {
      switch (newValue) {
        case 'collapse':
        case 'expand':
          iconButton?.setAttribute('iron-icon', 'cr:expand-less');
          break;
      }
    }
  }

  /**
   * When using the extra button, the text can be programmatically set
   * @param {string} text The text to use on the extra button.
   */
  setExtraButtonText(text) {
    if (!this.shadowRoot) {
      return;
    }
    if (isCrosComponentsEnabled()) {
      const extraButton =
          /** @type {!Button} */ (
              queryRequiredElement('#extra-button-jelly', this.shadowRoot));
      extraButton.label = text;
    } else {
      const extraButton =
          /** @type {!CrButtonElement} */ (
              queryRequiredElement('#extra-button', this.shadowRoot));
      extraButton.innerText = text;
    }
  }
}

window.customElements.define('xf-button', PanelButton);

// # sourceURL=//ui/file_manager/file_manager/foreground/elements/xf_button.js
