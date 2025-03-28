// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Test file for @webui-eslint/polymer-property-class-member

export class SomeElement extends PolymerElement {
  static override get properties() {
    return {
      // Case of Polymer property that is also declared as a class member, no
      // violation.
      prop1: {type: String},
      // Case of Polymer property that is also declared as a class member and
      // uses a function initializer, no violation.
      prop2: {
        type: String,
        value: function() {
          return {foo: 'foo', bar: 'bar'};
        },
      },
      // Case of Polymer property that is not declared as a class member,
      // violation.
      prop3: {type: String},
      // Case of Polymer property that is not declared as a class member, but
      // has the special 'Enum_`, no violation.
      someEnum_: {type: Object},
    };
  }

  declare prop1: string;
  declare prop2: string;
}

// Test the case where multiple PolymerElement classes exist in the same file.
export class SomeOtherElement extends PolymerElement {
  static override get properties() {
    return {
      // Case of Polymer property that is not declared as a class member,
      // violation.
      prop1: {type: String},
      // Case of Polymer property that is also declared as a class member and
      // uses a function initializer, no violation.
      prop2: {
        type: String,
        value: function() {
          return {foo: 'foo', bar: 'bar'};
        },
      },
      // Case of Polymer property that is also declared as a class member, no
      // violation.
      prop3: {type: String},
      // Case of Polymer property that is not declared as a class member, but
      // has the special 'Enum_`, no violation.
      someEnum_: {type: Object},
    };
  }

  declare prop2: string;
  declare prop3: string;
}
