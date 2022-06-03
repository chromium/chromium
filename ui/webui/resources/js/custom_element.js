// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for Web Components that don't use Polymer.
 * See the following file for usage:
 * chrome/test/data/webui/js/custom_element_test.js
 */
export class CustomElement extends HTMLElement {
  constructor() {
    super();

    this.attachShadow({mode: 'open'});
    const template = document.createElement('template');
    template.innerHTML = this.constructor.template || '';
    this.shadowRoot.appendChild(template.content.cloneNode(true));
  }

  /**
   * @param {string} query
   * @return {?Element}
   */
  $(query) {
    return this.shadowRoot.querySelector(query);
  }

  /**
   * @param {string} query
   * @return {!NodeList<!Element>}
   */
  $all(query) {
    return this.shadowRoot.querySelectorAll(query);
  }
}
