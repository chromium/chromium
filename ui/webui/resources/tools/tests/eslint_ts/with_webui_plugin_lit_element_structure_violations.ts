// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, css} from '//resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from '//resources/lit/v3_0/lit.rollup.js';

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
//  1) Inconsistent DOM name suffix.
//  2) Incorrect order method definition order
//  3) Usage of CustomEvent type without a type parameter.
export class TestError6Element extends CrLitElement {
  override render() {
    return '';
  }

  static override get styles() {
    return '';
  }

  static get is() {
    return 'test-error6-element';
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
  }

  override firstUpdated() {}

  // Test case where CustomEvent is used with a function parameter.
  protected onFoo_(someEvent: CustomEvent) {
    // Test case where CustomEvent is used with a type assertion.
    const _otherEvent = someEvent as CustomEvent;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error6-element': TestError6Element;
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

type Constructor<T> = new (...args: any[]) => T;
const FooMixin = <T extends Constructor<CrLitElement>>(superClass: T): T => {
  class FooMixin extends superClass {}
  return FooMixin;
};
const FooMixinLit = <T extends Constructor<CrLitElement>>(superClass: T): T => {
  class FooMixinLit extends superClass {}
  return FooMixinLit;
};

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

// Case1.11: Class with inconsistent class name.
export class TestError11Element extends CrLitElement {
  static get is() {
    return 'test-other-error11';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-other-error11': TestError11Element;
  }
}

// Case1.12: Invalid (optional/nullable/undefinable) properties in '$'
// interface.
export interface TestError12Element {
  $: {
    input: HTMLInputElement,
    nullableBtn: HTMLElement|null,
    undefinableBtn: HTMLElement|undefined,
    optionalBtn?: HTMLElement,
  };
}

export class TestError12Element extends CrLitElement {
  static get is() {
    return 'test-error12';
  }

  override render() {
    return html``;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error12': TestError12Element;
  }
}

customElements.define(TestError12Element.is, TestError12Element);

// Case1.13: Mixin class with missing superclass calls and incorrect method
// definition order.
export const TestError13Mixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T => {
      class TestError13Mixin extends superClass {
        override disconnectedCallback() {}
        override connectedCallback() {}
      }
      return TestError13Mixin;
    };

// Case1.14: Class extending an intermediate CrLitElement subclass (rather than
// CrLitElement directly).
export abstract class TestBaseLitElement extends CrLitElement {}
export class TestError14ElementFoo extends TestBaseLitElement {
  static get is() {
    return 'test-error14';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error14': TestError14ElementFoo;
  }
}

customElements.define(TestError14ElementFoo.is, TestError14ElementFoo);

// Case1.15: Re-exporting (directly or indirectly) from a file containing
// CrLitElement subclass definitions.
export {nothing} from '//resources/lit/v3_0/lit.rollup.js';
export type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
export {css};
export type {CSSResultGroup};

// Case1.16: Class with constructor parameters.
export class TestError16Element extends CrLitElement {
  static get is() {
    return 'test-error16';
  }

  constructor(_param: string) {
    super();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-error16': TestError16Element;
  }
}

customElements.define(TestError16Element.is, TestError16Element);

// Case1.17: Instantiating CrLitElement subclass with 'new'.
const _error17 = new TestError16Element('foo');


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
  }

  // Test case where CustomEvent is used with a function parameter.
  protected onFoo_(someEvent2: CustomEvent<string>) {
    // Test case where CustomEvent is used with a type assertion.
    const _otherEvent2 = someEvent2 as CustomEvent<string>;
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

// Case2.5: Exporting local declarations via 'export {...}' is allowed.
const localConst = 1;
interface LocalInterface {}
export {localConst};
export type {LocalInterface};

// Case2.6: Mixin class with rest constructor parameters (required by
// TypeScript TS2545 whenever a mixin class defines a constructor).
export const TestNoError6Mixin =
    <T extends Constructor<CrLitElement>>(superClass: T): T => {
      class TestNoError6Mixin extends superClass {
        constructor(...args: any[]) {
          super(...args);
        }
      }
      return TestNoError6Mixin;
    };

// Case2.7: Instantiating non-CrLitElement class with 'new' or CrLitElement
// subclass with document.createElement() is allowed.
const _noError7a = new TestNoError2Element();
const _noError7b = document.createElement('test-no-error3');
