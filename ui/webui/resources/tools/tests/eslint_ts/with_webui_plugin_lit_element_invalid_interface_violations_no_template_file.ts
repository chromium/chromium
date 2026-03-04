// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-incorrect-interface

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

interface MyDummyTestElement {
  $: {
    one: HTMLElement,
    two: HTMLElement,
    doesNotExistTest: HTMLElement,
  };
}

class MyDummyTestElement extends CrLitElement {
  static get is() {
    return 'test-dummy';
  }

  override render() {
    return html`<div id="one">Hello</div><div id="two">Test</div>`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-dummy': MyDummyTestElement;
  }
}

customElements.define(MyDummyTestElement.is, MyDummyTestElement);
