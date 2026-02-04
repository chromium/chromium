// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-element-structure

// Case1: Class missing all of 'static get is() {...}', HTMLElementTagNameMap
// and customElements.define() call.
export class SomeElement1 extends CrLitElement {}

// Case2: Class missing HTMLElementTagNameMap and customElements.define() call.
export class SomeElement2 extends CrLitElement {
  static get is() {
    return 'some-element2';
  }
}

// Case3: Class missing customElements.define() call.
export class SomeElement3 extends CrLitElement {
  static get is() {
    return 'some-element3';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'some-element3': SomeElement3;
  }
}

// Case4: Class presumably not missing anything, but typo exists.
export class SomeElement4 extends CrLitElement {
  static get is() {
    return 'some-element4';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'some-element4-typo': SomeElement4;  // <-- Typo in the DOM name exists.
  }
}

customElements.define(SomeElement4.is, SomeElement4);

// Case5: Class missing everything, but it is an abstract class, no violations.
export abstract class SomeElement5 extends CrLitElement {}

// Case6: Class missing everything, but it is not a CrLitElement subclass.
export class SomeElement6 extends HTMLEmbedElement {}

// Case7: Class not missing anything, no violations.
export class SomeElement7 extends CrLitElement {
  static get is() {
    return 'some-element7';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'some-element7': SomeElement7;
  }
}

customElements.define(SomeElement7.is, SomeElement7);
