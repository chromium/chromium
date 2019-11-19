// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Element which displays the number of selected items with
 * Cancel/Delete buttons, designed to be used as an overlay on top of
 * <cr-toolbar>. See <history-toolbar> for an example usage.
 *
 * Note that the embedder is expected to set position: relative to make the
 * absolute positioning of this element work, and the cr-toolbar should have the
 * has-overlay attribute set when its overlay is shown to prevent access through
 * tab-traversal.
 */

Polymer({
  is: 'cr-toolbar-selection-overlay',

  properties: {
    show: {
      type: Boolean,
      observer: 'onShowChanged_',
      reflectToAttribute: true,
    },

    deleteLabel: String,

    cancelLabel: String,

    selectionLabel: String,

    deleteDisabled: Boolean,

    /** @private */
    hasShown_: Boolean,

    /** @private */
    selectionLabel_: String,
  },

  observers: [
    'updateSelectionLabel_(show, selectionLabel)',
  ],

  /** @return {HTMLElement} */
  get deleteButton() {
    return /** @type {HTMLElement} */ (this.$$('#delete'));
  },

  /** @private */
  onClearSelectionClick_: function() {
    this.fire('clear-selected-items');
  },

  /** @private */
  onDeleteClick_: function() {
    this.fire('delete-selected-items');
  },

  /** @private */
  updateSelectionLabel_: function() {
    // Do this update in a microtask to ensure |show| and |selectionLabel|
    // are both updated.
    this.debounce('updateSelectionLabel_', () => {
      this.selectionLabel_ =
          this.show ? this.selectionLabel : this.selectionLabel_;
    });
  },

  /** @private */
  onShowChanged_: function() {
    if (this.show) {
      this.hasShown_ = true;
    }
  },
});
