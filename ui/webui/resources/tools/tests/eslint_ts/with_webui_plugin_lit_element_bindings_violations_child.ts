// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

export class HelloWorldChildElement extends CrLitElement {
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
