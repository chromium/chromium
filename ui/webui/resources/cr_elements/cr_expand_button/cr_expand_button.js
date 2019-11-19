// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around a button that toggles
 * between an opened (expanded) and closed state.
 */
Polymer({
  is: 'cr-expand-button',

  properties: {
    /**
     * If true, the button is in the expanded state and will show the
     * 'expand-less' icon. If false, the button shows the 'expand-more' icon.
     */
    expanded: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'onExpandedChange_',
    },

    /**
     * If true, the button will be disabled and grayed out.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** A11y text descriptor for this control. */
    alt: {
      type: String,
      observer: 'onAltChange_',
    },

    tabIndex: {
      type: Number,
      value: 0,
    },
  },

  observers: [
    'updateAriaExpanded_(disabled, expanded)',
  ],

  listeners: {
    click: 'toggleExpand_',
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

  /** @private */
  onAltChange_: function() {
    if (this.alt) {
      this.$.icon.removeAttribute('aria-labelledby');
      this.$.icon.setAttribute('aria-label', this.alt);
    } else {
      this.$.icon.removeAttribute('aria-label');
      this.$.icon.setAttribute('aria-labelledby', 'label');
    }
  },

  /** @private */
  onExpandedChange_: function() {
    this.$.icon.ironIcon = this.expanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @param {!Event} event
   * @private
   */
  toggleExpand_: function(event) {
    // Prevent |click| event from bubbling. It can cause parents of this
    // elements to erroneously re-toggle this control.
    event.stopPropagation();
    event.preventDefault();

    this.scrollIntoViewIfNeeded();
    this.expanded = !this.expanded;
    cr.ui.focusWithoutInk(this.$.icon);
  },

  /** @private */
  updateAriaExpanded_: function() {
    if (this.disabled) {
      this.$.icon.removeAttribute('aria-expanded');
    } else {
      this.$.icon.setAttribute('aria-expanded', this.expanded);
    }
  },
});
