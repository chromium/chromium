// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-element-structure

/* Cases with violations below. */

// Case1.1: Class missing all of 'static get is() {...}', HTMLElementTagNameMap
// and customElements.define() call.
export class TestError1Element extends CrLitElement {}

// Case1.2: Class missing HTMLElementTagNameMap and customElements.define()
// call.
export class TestError2Element extends CrLitElement {
  static get is() {
    return 'test-error2';
  }
}

// Case1.3: Class missing customElements.define() call.
export class TestError3Element extends CrLitElement {
  static get is() {
    return 'test-error3';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error3': TestError3Element;
  }
}

// Case1.4: Class presumably not missing anything, but typo exists.
export class TestError4Element extends CrLitElement {
  static get is() {
    return 'test-error4';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error4-typo': TestError4Element;  // <-- Typo in the DOM name exists.
  }
}

customElements.define(TestError4Element.is, TestError4Element);

// Case1.5: Class missing superclass calls for lifecycle methods.
export class TestError5Element extends CrLitElement {
  static get is() {
    return 'test-error5';
  }

  override connectedCallback() {}
  override disconnectedCallback() {}
  override willUpdate() {}
  override updated() {}
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error5': TestError5Element;
  }
}

customElements.define(TestError5Element.is, TestError5Element);

// Case1.6: Class with
//  1) Incorrect order method definition order
//  2) Usage of this.dispatchEvent(new CustomEvent(...))
//  3) Usage of incorrect dollar sign notation.
export class TestError6Element extends CrLitElement {
  override render() {
    return '';
  }

  static override get styles() {
    return '';
  }

  static get is() {
    return 'test-error6';
  }

  static override get properties() {
    return {};
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  constructor() {
    super();
  }

  override willUpdate() {
    super.willUpdate();
  }

  override updated() {
    super.updated();

    this.dispatchEvent(
        new CustomEvent('foo1-updated', {bubbles: true, composed: true}));
    this.dispatchEvent(new CustomEvent(
        'foo2-updated', {bubbles: true, composed: true, detail: 'foo'}));

    const FOO3_UPDATED = 'foo3-updated';
    this.dispatchEvent(new CustomEvent(
        FOO3_UPDATED, {bubbles: true, composed: true, detail: 'foo'}));

    this.$['hello-button'].focus();
  }

  override firstUpdated() {}
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error6': TestError6Element;
  }
}

customElements.define(TestError6Element.is, TestError6Element);

// Case1.7: Class with incorrect class name, using "extends CrLitElement".
export class TestError7ElementFoo extends CrLitElement {
  static get is() {
    return 'test-error7';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error7': TestError7ElementFoo;
  }
}

customElements.define(TestError7ElementFoo.is, TestError7ElementFoo);

// Case1.8: Class with incorrect class name, using "extends
// FooMixin(CrLitElement)".
export class TestError8ElementFoo extends FooMixin
(CrLitElement) {
  static get is() {
    return 'test-error8';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error8': TestError8ElementFoo;
  }
}

customElements.define(TestError8ElementFoo.is, TestError8ElementFoo);

// Case1.9: Class with incorrect class name, using "extends
// FooMixinLit(CrLitElement)" (note the "Lit" suffix in the Mixin name).
export class TestError9ElementFoo extends FooMixinLit
(CrLitElement) {
  static get is() {
    return 'test-error9';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error9': TestError9ElementFoo;
  }
}

customElements.define(TestError9ElementFoo.is, TestError9ElementFoo);

// Case1.10: Class with incorrect class name, using "extends
// TestError10ElementFooBase".
const TestError10ElementFooBase = FooMixin(CrLitElement);
export class TestError10ElementFoo extends TestError10ElementFooBase {
  static get is() {
    return 'test-error10';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error10': TestError10ElementFoo;
  }
}

customElements.define(TestError10ElementFoo.is, TestError10ElementFoo);


/* Cases with no violations below. */

// Case2.1: Class missing everything, but it is an abstract class.
export abstract class TestNoError1Element extends CrLitElement {}

// Case2.2: Class missing everything, but it is not a CrLitElement subclass.
export class TestNoError2Element extends HTMLEmbedElement {}

// Case2.3: Class not missing anything.
export class TestNoError3Element extends CrLitElement {
  static get is() {
    return 'test-no-error3';
  }

  static override get styles() {
    return '';
  }

  override render() {
    return '';
  }

  static override get properties() {
    return {};
  }

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  override willUpdate() {
    super.willUpdate();
  }

  override firstUpdated() {}

  override updated() {
    super.updated();

    // Case where bubbles, composed are not specified.
    this.dispatchEvent(new CustomEvent('bar-updated', {detail: 'bar'}));
    // Case where bubbles, composed are specified but one of them is false.
    this.dispatchEvent(new CustomEvent(
        'bar-updated', {bubbles: true, composed: false, detail: 'bar'}));
    // Case where options besides bubbles, composed, detail are specifiied.
    this.dispatchEvent(new CustomEvent(
        'bar-updated',
        {bubbles: true, composed: true, cancelable: true, detail: 'bar'}));

    this.fire('bar-updated', 'bar');
    this.$.helloOtherButton.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-no-error3': TestNoError3Element;
  }
}

customElements.define(TestNoError3Element.is, TestNoError3Element);

// Case2.4: Class without the "Element" suffix, but it is not CrLitElement
// subclass.
export class TestNoError4ElementFoo extends SomeOtherElement {}

declare global {
  interface HTMLElementTagNameMap {
    'test-no-error4': TestNoError4ElementFoo;
  }
}

customElements.define(TestNoError4ElementFoo.is, TestNoError4ElementFoo);
