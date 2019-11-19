// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 */
Polymer({
  is: 'cr-link-row',

  properties: {
    startIcon: {
      type: String,
      value: '',
    },

    label: {
      type: String,
      value: '',
    },

    subLabel: {
      type: String,
      /* Value used for noSubLabel attribute. */
      value: '',
    },

    disabled: {
      type: Boolean,
      reflectToAttribute: true,
    },

    external: {
      type: Boolean,
      value: false,
    },

    usingSlottedLabel: {
      type: Boolean,
      value: false,
    },

    roleDescription: String,

    /** @private */
    hideLabelWrapper_: {
      type: Boolean,
      computed: 'computeHideLabelWrapper_(label, usingSlottedLabel)',
    },
  },

  /** @type {boolean} */
  get noink() {
    return this.$.icon.noink;
  },

  /** @type {boolean} */
  set noink(value) {
    this.$.icon.noink = value;
  },

  focus: function() {
    this.$.icon.focus();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHideLabelWrapper_: function() {
    return !(this.label || this.usingSlottedLabel);
  },

  /**
   * @return {string}
   * @private
   */
  getIcon_: function() {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  },
});
