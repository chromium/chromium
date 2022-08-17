// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '../hidden_style_css.m.js';
import '../shared_vars_css.m.js';
import './cr_radio_button_style.css.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonBehavior} from './cr_radio_button_behavior.js';

Polymer({
  is: 'cr-radio-button',

  _template: html`{__html_template__}`,

  behaviors: [
    CrRadioButtonBehavior,
  ],
});
