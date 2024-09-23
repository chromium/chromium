// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

/**
 * @fileoverview Base class for Web Components that don't use Polymer.
 * See the following file for usage:
 * chrome/test/data/webui/js/custom_element_test.js
 */

function emptyHTML(): string|TrustedHTML {
  return window.trustedTypes ? window.trustedTypes.emptyHTML : '';
}

export class CustomElement extends HTMLElement {
  static get template() {
    return emptyHTML();
  }

  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML =
        (this.constructor as typeof CustomElement).template || emptyHTML();
    this.shadowRoot!.appendChild(template.content.cloneNode(true));
  }

  $<K extends keyof HTMLElementTagNameMap>(query: K):
      HTMLElementTagNameMap[K]|null;
  $<E extends HTMLElement = HTMLElement>(query: string): E|null;
  $(query: string) {
    return this.shadowRoot!.querySelector(query);
  }

  $all<K extends keyof HTMLElementTagNameMap>(selectors: K):
      NodeListOf<HTMLElementTagNameMap[K]>;
  $all<E extends Element = Element>(selectors: string): NodeListOf<E>;
  $all(query: string) {
    return this.shadowRoot!.querySelectorAll(query);
  }

  getRequiredElement<K extends keyof HTMLElementTagNameMap>(query: K):
      HTMLElementTagNameMap[K];
  getRequiredElement<E extends HTMLElement = HTMLElement>(query: string): E;
  getRequiredElement(query: string) {
    const el = this.shadowRoot!.querySelector(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }
}
