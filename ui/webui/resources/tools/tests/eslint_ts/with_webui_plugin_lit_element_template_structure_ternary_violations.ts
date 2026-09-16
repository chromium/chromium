// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

export class TernaryDummyElement extends CrLitElement {
  static get is() {
    return 'ternary-dummy';
  }

  static get properties() {
    return {
      condition1: {type: Boolean},
      condition2: {type: Boolean},
    };
  }

  condition1: boolean = false;
  condition2: boolean = false;
}
