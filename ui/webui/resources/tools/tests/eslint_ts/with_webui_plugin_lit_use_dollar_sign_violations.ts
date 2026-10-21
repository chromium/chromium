// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-use-dollar-sign

export interface DollarSignViolationsElement {
  $: {
    myButton: HTMLElement,
  };
}

export class DollarSignViolationsElement extends CrLitElement {
  static get is() {
    return 'dollar-sign-violations';
  }

  override render() {
    return html`
      <div id="myButton"></div>
    `;
  }

  someMethod() {
    this.shadowRoot.querySelector('#myButton');
    this.shadowRoot?.querySelector('#myButton');

    // Negative cases on `this`:
    this.shadowRoot.querySelector('#unknownButton');
    this.shadowRoot.querySelector('.someClass');
  }
}

customElements.define(
    DollarSignViolationsElement.is, DollarSignViolationsElement);

declare global {
  interface HTMLElementTagNameMap {
    'dollar-sign-violations': DollarSignViolationsElement;
  }
}

export class NonLitElement extends HTMLElement {
  someMethod() {
    this.shadowRoot!.querySelector('#myButton');
  }
}

declare const childElement: DollarSignViolationsElement;
declare const nonLitElement: NonLitElement;
declare const anyElement: any;

/* Cases with violations below. */

childElement.shadowRoot.querySelector('#myButton');
childElement.shadowRoot?.querySelector<HTMLElement>('#myButton');

/* Cases with no violations below. */

childElement.shadowRoot.querySelector('#unknownButton');
childElement.querySelector('#myButton');
childElement.shadowRoot.querySelector('.someClass');
childElement.shadowRoot.querySelector('#myButton .child');
nonLitElement.shadowRoot!.querySelector('#myButton');
anyElement.shadowRoot.querySelector('#myButton');
