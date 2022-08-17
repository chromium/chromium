// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-card-radio-button' is a radio button in the style of a card. A checkmark
 * is displayed in the upper right hand corner if the radio button is selected.
 */
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cr_radio_button_style.css.js';
import '../icons.m.js';
import '../shared_vars_css.m.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonBehavior} from './cr_radio_button_behavior.js';

Polymer({
  _template: html`{__html_template__}`,

  is: 'cr-card-radio-button',

  behaviors: [
    CrRadioButtonBehavior,
  ],

  onFocus_() {},
});
