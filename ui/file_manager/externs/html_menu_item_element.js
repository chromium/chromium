// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#the-menuitem-element
 */
class HTMLMenuItemElement extends HTMLElement {
  constructor() {
    /**
     * @type {string}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-type
     */
    this.type;

    /**
     * @type {string}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-label
     */
    this.label;

    /**
     * @type {string}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-icon
     */
    this.icon;

    /**
     * @type {boolean}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-disabled
     */
    this.disabled;

    /**
     * @type {boolean}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-checked
     */
    this.checked;

    /**
     * @type {string}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-radiogroup
     */
    this.radiogroup;

    /**
     * @type {boolean}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-default
     */
    this.default;

    /**
     * @type {HTMLElement|undefined}
     * @see http://www.w3.org/html/wg/drafts/html/master/interactive-elements.html#dom-menuitem-command
     */
    this.command;
  }
}
