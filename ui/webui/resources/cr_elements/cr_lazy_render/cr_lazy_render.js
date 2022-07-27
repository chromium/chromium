// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-lazy-render is a simple variant of dom-if designed for lazy rendering
 * of elements that are accessed imperatively.
 * Usage:
 *   <cr-lazy-render id="menu">
 *     <template>
 *       <heavy-menu></heavy-menu>
 *     </template>
 *   </cr-lazy-render>
 *
 *   this.$.menu.get().show();
 */

import {html, Polymer, TemplateInstanceBase, templatize} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'cr-lazy-render',

  _template: html`{__html_template__}`,

  /** @private {?Element} */
  child_: null,

  /** @private {?Element|?TemplateInstanceBase} */
  instance_: null,

  /**
   * Stamp the template into the DOM tree synchronously
   * @return {Element} Child element which has been stamped into the DOM tree.
   */
  get() {
    if (!this.child_) {
      this.render_();
    }
    return this.child_;
  },

  /**
   * @return {?Element} The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists() {
    return this.child_;
  },

  /** @private */
  render_() {
    const template =
        /** @type {!HTMLTemplateElement} */ (this.getContentChildren()[0]);
    const TemplateClass = templatize(template, this, {
      mutableData: false,
      forwardHostProp: this._forwardHostPropV2,
    });
    const parentNode = this.parentNode;
    if (parentNode && !this.child_) {
      this.instance_ = new TemplateClass();
      this.child_ = this.instance_.root.firstElementChild;
      parentNode.insertBefore(this.instance_.root, this);
    }
  },

  /**
   * @param {string} prop
   * @param {Object} value
   */
  _forwardHostPropV2(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
