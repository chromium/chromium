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

  $<E extends Element = Element>(query: string): E|null {
    return this.shadowRoot!.querySelector<E>(query);
  }

  $all<E extends Element = Element>(query: string): NodeListOf<E> {
    return this.shadowRoot!.querySelectorAll<E>(query);
  }

  getRequiredElement<T extends HTMLElement = HTMLElement>(query: string): T {
    const el = this.shadowRoot!.querySelector<T>(query);
    assert(el);
    assert(el instanceof HTMLElement);
    return el;
  }
}
