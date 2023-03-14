// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from '//resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrRadioGroupElement extends LegacyElementMixin, HTMLElement {
  disabled: boolean;
  selected: string;
  selectableElements: string;
}

export {CrRadioGroupElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-radio-group': CrRadioGroupElement;
  }
}
