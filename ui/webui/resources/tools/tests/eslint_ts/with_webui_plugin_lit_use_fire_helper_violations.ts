// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

// Test file for @webui-eslint/lit-use-fire-helper

type Constructor<T> = new (...args: any[]) => T;
interface TestMixinInterface {
  mixinProp: string;
}
const TestMixin = <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<TestMixinInterface> => {
      class TestMixin extends superClass implements TestMixinInterface {
        mixinProp: string = 'foo';
      }
      return TestMixin;
    };

export class FireHelperViolationsOneElement extends TestMixin
(CrLitElement) {
  static get is() {
    return 'fire-helper-violations-one';
  }
}
customElements.define(
    FireHelperViolationsOneElement.is, FireHelperViolationsOneElement);

export class FireHelperViolationsTwoElement extends CrLitElement {
  static get is() {
    return 'fire-helper-violations-two';
  }

  override updated() {
    super.updated();

    this.dispatchEvent(
        new CustomEvent('this1-updated', {bubbles: true, composed: true}));
    this.dispatchEvent(new CustomEvent(
        'this2-updated', {bubbles: true, composed: true, detail: 'foo'}));

    const THIS3_UPDATED = 'this3-updated';
    this.dispatchEvent(new CustomEvent(
        THIS3_UPDATED, {bubbles: true, composed: true, detail: 'foo'}));

    // Cases with no violations on `this`:
    // Case where bubbles, composed are not specified.
    this.dispatchEvent(new CustomEvent('this-bar-updated', {detail: 'bar'}));
    // Case where bubbles, composed are specified but one of them is false.
    this.dispatchEvent(new CustomEvent(
        'this-bar-updated', {bubbles: true, composed: false, detail: 'bar'}));
    // Case where options besides bubbles, composed, detail are specified.
    this.dispatchEvent(new CustomEvent(
        'this-bar-updated',
        {bubbles: true, composed: true, cancelable: true, detail: 'bar'}));
  }
}

customElements.define(
    FireHelperViolationsTwoElement.is, FireHelperViolationsTwoElement);

declare global {
  interface HTMLElementTagNameMap {
    'fire-helper-violations-one': FireHelperViolationsOneElement;
    'fire-helper-violations-two': FireHelperViolationsTwoElement;
  }
}

declare const parentElement: FireHelperViolationsTwoElement;
declare const childElement: FireHelperViolationsOneElement;
declare const htmlElement: HTMLElement;
declare const anyElement: any;

/* Cases with violations below. */

childElement.dispatchEvent(
    new CustomEvent('foo1-updated', {bubbles: true, composed: true}));

parentElement.dispatchEvent(new CustomEvent(
    'foo2-updated', {bubbles: true, composed: true, detail: 'foo'}));

const FOO3_UPDATED = 'foo3-updated';
parentElement.dispatchEvent(new CustomEvent(
    FOO3_UPDATED, {bubbles: true, composed: true, detail: 'foo'}));

const queriedChild = document.querySelector('fire-helper-violations-one');
queriedChild!.dispatchEvent(
    new CustomEvent('foo4-updated', {bubbles: true, composed: true}));


/* Cases with no violations below. */

export class NonLitElement extends HTMLElement {
  someMethod() {
    this.dispatchEvent(new CustomEvent(
        'non-lit-this-updated',
        {bubbles: true, composed: true, detail: 'bar'}));
  }
}

// Not a CrLitElement subclass (HTMLElement, Window, any).
htmlElement.dispatchEvent(new CustomEvent(
    'bar1-updated', {bubbles: true, composed: true, detail: 'bar'}));
window.dispatchEvent(
    new CustomEvent('bar2-updated', {bubbles: true, composed: true}));
anyElement.dispatchEvent(
    new CustomEvent('bar3-updated', {bubbles: true, composed: true}));

// Case where bubbles, composed are not specified.
childElement.dispatchEvent(new CustomEvent('bar4-updated', {detail: 'bar'}));

// Case where bubbles, composed are specified but one of them is false.
childElement.dispatchEvent(new CustomEvent(
    'bar5-updated', {bubbles: true, composed: false, detail: 'bar'}));

// Case where options besides bubbles, composed, detail are specified.
childElement.dispatchEvent(new CustomEvent(
    'bar6-updated',
    {bubbles: true, composed: true, cancelable: true, detail: 'bar'}));
