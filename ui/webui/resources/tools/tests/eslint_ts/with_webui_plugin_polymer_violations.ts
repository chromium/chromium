// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Test file for @webui-eslint/polymer-property-declare

export class SomeElement extends PolymerElement {
  static override get properties() {
    return {
      prop1: {type: String},
      prop2: {type: String},
    };
  }

  // Case of Polymer property with the 'declare' keyword, no error expected.
  declare prop1: string;
  // Case of Polymer property without the 'declare' keyword, error expected.
  prop2: string;
  // Case of non Polymer property with the 'declare keyword, error expected.
  declare prop3: string;
  // Case of non Polymer property without the 'declare keyword, no error
  // expected.
  prop4: string;
}

// Test the case where multiple PolymerElement classes exist in the same file.
export class SomeOtherElement extends PolymerElement {
  static override get properties() {
    return {
      prop1: {type: String},
      prop2: {type: String},
    };
  }

  // Case of Polymer property without the 'declare' keyword, error expected.
  prop1: string;
  // Case of Polymer property with the 'declare' keyword, no error expected.
  declare prop2: string;
  // Case of non Polymer property without the 'declare keyword, no error
  // expected.
  prop3: string;
  // Case of non Polymer property with the 'declare keyword, error expected.
  declare prop4: string;
}
