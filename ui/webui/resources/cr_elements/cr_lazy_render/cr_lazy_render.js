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

Polymer({
  is: 'cr-lazy-render',

  /** @private {?Element} */
  child_: null,

  /** @private {?Element|?TemplateInstanceBase} */
  instance_: null,

  /**
   * Stamp the template into the DOM tree synchronously
   * @return {Element} Child element which has been stamped into the DOM tree.
   */
  get: function() {
    if (!this.child_) {
      this.render_();
    }
    return this.child_;
  },

  /**
   * @return {?Element} The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists: function() {
    return this.child_;
  },

  /** @private */
  render_: function() {
    const template =
        /** @type {!HTMLTemplateElement} */ (this.getContentChildren()[0]);
    const TemplateClass = Polymer.Templatize.templatize(template, this, {
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
  _forwardHostPropV2: function(prop, value) {
    if (this.instance_) {
      this.instance_.forwardHostProp(prop, value);
    }
  },
});
