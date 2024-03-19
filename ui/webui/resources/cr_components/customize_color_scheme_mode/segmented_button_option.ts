// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PaperRippleMixin} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-mixin.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonMixin} from '//resources/cr_elements/cr_radio_button/cr_radio_button_mixin.js';

import {getTemplate} from './segmented_button_option.html.js';

const SegmentedButtonElementBase =
    PaperRippleMixin(CrRadioButtonMixin(PolymerElement));

export class SegmentedButtonOptionElement extends
    SegmentedButtonElementBase {
  static get is() {
    return 'segmented-button-option';
  }

  static get template() {
    return getTemplate();
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from PaperRippleMixin
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.shadowRoot!.querySelector('#button');
    return super._createRipple();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'segmented-button-option': SegmentedButtonOptionElement;
  }
}

customElements.define(
    SegmentedButtonOptionElement.is, SegmentedButtonOptionElement);
