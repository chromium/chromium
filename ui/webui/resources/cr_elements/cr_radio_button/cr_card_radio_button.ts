// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-card-radio-button' is a radio button in the style of a card. A checkmark
 * is displayed in the upper right hand corner if the radio button is selected.
 */
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cr_radio_button_style.css.js';
import '../cr_shared_vars.css.js';
import '../icons.html.js';

import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';

import {getTemplate} from './cr_card_radio_button.html.js';
import {CrRadioButtonMixin, CrRadioButtonMixinInterface} from './cr_radio_button_mixin.js';

const CrCardRadioButtonElementBase =
    mixinBehaviors([PaperRippleBehavior], CrRadioButtonMixin(PolymerElement)) as
    {
      new (): PolymerElement & CrRadioButtonMixinInterface &
          PaperRippleBehavior,
    };

export interface CrCardRadioButtonElement {
  $: {
    button: HTMLElement,
  };
}

export class CrCardRadioButtonElement extends CrCardRadioButtonElementBase {
  static get is() {
    return 'cr-card-radio-button';
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
    this._rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-card-radio-button': CrCardRadioButtonElement;
  }
}

customElements.define(CrCardRadioButtonElement.is, CrCardRadioButtonElement);
