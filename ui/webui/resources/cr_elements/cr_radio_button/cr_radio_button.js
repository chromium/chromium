// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '../hidden_style_css.m.js';
import '../shared_vars_css.m.js';
import './cr_radio_button_style.css.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonBehavior, CrRadioButtonBehaviorInterface} from './cr_radio_button_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrRadioButtonBehaviorInterface}
 */
const CrRadioButtonElementBase =
    mixinBehaviors([CrRadioButtonBehavior], PolymerElement);

/** @polymer */
export class CrRadioButtonElement extends CrRadioButtonElementBase {
  static get is() {
    return 'cr-radio-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(CrRadioButtonElement.is, CrRadioButtonElement);
