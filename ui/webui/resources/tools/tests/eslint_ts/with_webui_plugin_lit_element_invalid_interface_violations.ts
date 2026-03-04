// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-incorrect-interface

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_lit_element_incorrect_interface_violations.html.js';

export interface MyDummyElement {
  $: {
    one: HTMLElement,
    two: HTMLElement,
    'three': HTMLElement,
    'four-four': HTMLElement,
    doesNotExist: HTMLElement,
  };
}

export class MyDummyElement extends CrLitElement {
  static get is() {
    return 'test-dummy';
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-dummy': MyDummyElement;
  }
}

customElements.define(MyDummyElement.is, MyDummyElement);
