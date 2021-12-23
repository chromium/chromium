// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';
import {CrRadioButtonBehavior} from './cr_radio_button_behavior.m.js';

interface CrCardRadioButtonElement extends LegacyElementMixin,
                                           CrRadioButtonBehavior, HTMLElement {
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
