// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem
 */
class HTMLMenuItemElement extends HTMLElement {
  constructor() {
    /**
     * @type {string}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-type
     */
    this.type;

    /**
     * @type {string}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-label
     */
    this.label;

    /**
     * @type {string}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-icon
     */
    this.icon;

    /**
     * @type {boolean}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-disabled
     */
    this.disabled;

    /**
     * @type {boolean}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-checked
     */
    this.checked;

    /**
     * @type {string}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-radiogroup
     */
    this.radiogroup;

    /**
     * @type {boolean}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-default
     */
    this.default;

    /**
     * @type {HTMLElement|undefined}
     * @see https://developer.mozilla.org/en-US/docs/Web/HTML/Element/menuitem#attr-command
     */
    this.command;
  }
}
