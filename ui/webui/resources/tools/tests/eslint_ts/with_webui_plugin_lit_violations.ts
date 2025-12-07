// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-property-accessor

export class SomeElement extends CrLitElement {
  static override get properties() {
    return {
      prop1: {type: String},
      prop2: {type: String},
    };
  }

  // Case of Lit reactive property with the 'accessor' keyword, no error
  // expected.
  accessor prop1: string = 'prop1';
  // Case of Lit reactive property without the 'accessor' keyword, error
  // expected.
  prop2: string = 'prop2';
  // Case of non Lit-reactive property with the 'accessor keyword, error
  // expected.
  accessor prop3: string = 'prop3';
  // Case of non Lit-reactive property without the 'accessor keyword, no error
  // expected.
  prop4: string = 'prop4';
}

// Test the case where multiple CrLitElement classes exist in the same file.
export class SomeOtherElement extends CrLitElement {
  static override get properties() {
    return {
      prop1: {type: String},
      prop2: {type: String},
    };
  }

  // Case of Lit reactive property without the 'accessor' keyword, error
  // expected.
  prop1: string = 'prop1';
  // Case of Lit reactive property with the 'accessor' keyword, no error
  // expected.
  accessor prop2: string = 'prop2';
  // Case of non Lit-reactive property without the 'accessor keyword, no error
  // expected.
  prop3: string = 'prop3';
  // Case of non Lit-reactive property with the 'accessor keyword, error
  // expected.
  accessor prop4: string = 'prop4';
}
