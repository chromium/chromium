// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-incorrect-interface

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_lit_element_bindings_violations.html.js';

export class HelloWorldDummyElement extends CrLitElement {
  static get is() {
    return 'hello-world-dummy';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {
        type: Boolean,
        reflect: true,
      },
      description: {type: Object},
      value: {type: Array},
      limits: {type: Object},
      label: {type: String},
      errorMessage: {type: String},
    };
  }

  accessor disabled: boolean = false;
  accessor value: number[] = [0];
  accessor errorMessage: string = '';
  accessor label: string = 'hello world';
  accessor description:
      {[key: string]: string} = {'is': 'input', 'controls': 'count'};
  accessor limits: {min: number, max: number} = {min: 0, max: 10};
}

declare global {
  interface HTMLElementTagNameMap {
    'hello-world-dummy': HelloWorldDummyElement;
  }
}

// Testing to ensure errors are still caught when aliasing is used.
export type DummyElement = HelloWorldDummyElement;

customElements.define(HelloWorldDummyElement.is, HelloWorldDummyElement);
