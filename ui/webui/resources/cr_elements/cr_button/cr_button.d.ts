// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

interface CrButtonElement extends PolymerElement {
  disabled: boolean;
  customTabIndex: number|null|undefined;
  hostAttributes: object|null;
}

export {CrButtonElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-button': CrButtonElement;
  }
}
