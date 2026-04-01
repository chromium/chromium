// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '/resources/lit/v3_0/lit.rollup.js';

import {MyTestMixin} from './with_webui_plugin_lit_element_bindings_violations_mixin.js';
import type {MyTestMixinInterface} from './with_webui_plugin_lit_element_bindings_violations_mixin.js';

// Not needed in production, but appears to be needed in tests to get
// TS compiler to understand the type of MyTestMixin.
export interface HelloWorldChildElement extends MyTestMixinInterface {}

export class HelloWorldChildElement extends MyTestMixin
(CrLitElement) {
  static get is() {
    return 'hello-world-child';
  }

  static override get properties() {
    return {
      fooString: {type: String},
      fooNumber: {type: Number},
      fooBoolean: {type: Boolean},
      fooArray: {type: Array},
      fooObject: {type: Object},
    };
  }

  accessor fooString: string = '';
  accessor fooNumber: number = 0;
  accessor fooBoolean: boolean = false;
  accessor fooArray: number[] = [];
  accessor fooObject: {bar: string} = {bar: ''};
}

declare global {
  interface HTMLElementTagNameMap {
    'hello-world-child': HelloWorldChildElement;
  }
}

customElements.define(HelloWorldChildElement.is, HelloWorldChildElement);
