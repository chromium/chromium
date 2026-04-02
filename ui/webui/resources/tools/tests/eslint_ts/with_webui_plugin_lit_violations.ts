// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-property-accessor

enum StringEnum {
  A = 'a',
  B = 'b',
}

enum NumberEnum {
  ONE = 1,
  TWO = 2,
}

export class SomeElement extends CrLitElement {
  static override get properties() {
    return {
      prop1: {type: String},
      prop2: {type: String},
      propMismatchedString: {type: String},
      propMismatchedNumber: {type: Number},
      propEnumString: {type: String},
      propEnumNumber: {type: Number},
      propMissingNumber: {type: Number},
      propObjectType: {type: Object},
    };
  }

  // Case of Lit reactive property with the 'accessor' keyword, no error
  // expected.
  accessor prop1: string = 'prop1';
  // Case of Lit reactive property without the 'accessor' keyword, error
  // expected.
  prop2: string = 'prop2';

  // Cases of Lit reactive property type mismatches, error expected.
  accessor propMismatchedString: number = 0;
  accessor propMismatchedNumber: string = '';

  // Cases of Lit reactive properties typed as enums in TS, no error.
  accessor propEnumString: StringEnum = StringEnum.A;
  accessor propEnumNumber: NumberEnum = NumberEnum.ONE;

  // Case of Lit reactive property typed as Object with a specific type in TS,
  // no error expected.
  accessor propObjectType: {foo: string} = {foo: ''};

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
