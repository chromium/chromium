// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-incorrect-interface

import './with_webui_plugin_lit_element_bindings_violations_child.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_lit_element_bindings_violations.html.js';

export class LitElementBindingsViolationsElement extends CrLitElement {
  static get is() {
    return 'lit-element-bindings-violations';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      buttonDisabled: {type: Boolean},
      disabled: {
        type: Boolean,
        reflect: true,
      },
      description: {type: Object},
      value: {type: Array},
      limits: {type: Object},
      label: {type: String},
      errorMessage: {type: String},
      someArrayProp: {type: Array},
      onClick_: {type: Array},
    };
  }

  accessor buttonDisabled: boolean|undefined;
  accessor disabled: boolean = false;
  accessor value: number[] = [0];
  accessor errorMessage: string = '';
  accessor label: string = 'hello world';
  accessor someArrayProp: string = '';
  accessor onClick_: number[] = [0];
  trustedHtml: TrustedHTML = window.trustedTypes!.emptyHTML;

  getErrorMessage(): string {
    return 'some error';
  }

  getLabels() {
    return ['label1', 'label2'];
  }

  onClick() {}

  accessor description:
      {[key: string]: string} = {'is': 'input', 'controls': 'count'};
  accessor limits: {min: number, max: number} = {min: 0, max: 10};
}

declare global {
  interface HTMLElementTagNameMap {
    'lit-element-bindings-violations': LitElementBindingsViolationsElement;
  }
}

// Testing to ensure errors are still caught when aliasing is used.
export type BindingsViolationsElement = LitElementBindingsViolationsElement;

customElements.define(
    LitElementBindingsViolationsElement.is,
    LitElementBindingsViolationsElement);
