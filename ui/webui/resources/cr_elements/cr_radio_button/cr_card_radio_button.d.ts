// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrRadioButtonBehavior} from './cr_radio_button_behavior.js';

interface CrCardRadioButtonElement extends CrRadioButtonBehavior,
                                           PolymerElement {
  $: {
    button: HTMLElement,
  };
}

export {CrCardRadioButtonElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-card-radio-button': CrCardRadioButtonElement;
  }
}
