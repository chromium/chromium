// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {PaperRippleBehavior} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonMixin, CrRadioButtonMixinInterface} from '../cr_radio_button/cr_radio_button_mixin.js';

import {getTemplate} from './cr_segmented_button_option.html.js';

const CrSegmentedButtonElementBase =
    mixinBehaviors([PaperRippleBehavior], CrRadioButtonMixin(PolymerElement)) as
    {
      new (): PolymerElement & CrRadioButtonMixinInterface &
          PaperRippleBehavior,
    };

export class CrSegmentedButtonOptionElement extends
    CrSegmentedButtonElementBase {
  static get is() {
    return 'cr-segmented-button-option';
  }

  static get template() {
    return getTemplate();
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from PaperRippleBehavior
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.shadowRoot!.querySelector('#button');
    return super._createRipple();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-segmented-button-option': CrSegmentedButtonOptionElement;
  }
}

customElements.define(
    CrSegmentedButtonOptionElement.is, CrSegmentedButtonOptionElement);
