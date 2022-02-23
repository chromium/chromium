// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for Web Components that don't use Polymer.
 * See the following file for usage:
 * chrome/test/data/webui/js/custom_element_test.js
 */

export class CustomElement extends HTMLElement {
  static get template(): string {
    return '';
  }

  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML =
        (this.constructor as typeof CustomElement).template || '';
    this.shadowRoot!.appendChild(template.content.cloneNode(true));
  }

  $<E extends Element = Element>(query: string): E|null {
    return this.shadowRoot!.querySelector<E>(query);
  }

  $all<E extends Element = Element>(query: string): NodeListOf<E> {
    return this.shadowRoot!.querySelectorAll<E>(query);
  }
}
