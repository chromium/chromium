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

import {Polymer, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {IronA11yAnnouncer} from '//resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import '../cr_button/cr_button.m.js';
import '../cr_icon_button/cr_icon_button.m.js';
import '../icons.m.js';
import '../shared_vars_css.m.js';

Polymer({
  is: 'cr-toolbar-selection-overlay',

  _template: html`{__html_template__}`,

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

  hostAttributes: {
    'role': 'toolbar',
  },

  /** @return {HTMLElement} */
  get deleteButton() {
    return /** @type {HTMLElement} */ (this.$$('#delete'));
  },

  /** @private */
  onClearSelectionClick_() {
    this.fire('clear-selected-items');
  },

  /** @private */
  onDeleteClick_() {
    this.fire('delete-selected-items');
  },

  /** @private */
  updateSelectionLabel_() {
    // Do this update in a microtask to ensure |show| and |selectionLabel|
    // are both updated.
    this.debounce('updateSelectionLabel_', () => {
      this.selectionLabel_ =
          this.show ? this.selectionLabel : this.selectionLabel_;
      this.setAttribute('aria-label', this.selectionLabel_);

      IronA11yAnnouncer.requestAvailability();
      this.fire('iron-announce', {text: this.selectionLabel});
    });
  },

  /** @private */
  onShowChanged_() {
    if (this.show) {
      this.hasShown_ = true;
    }
  },
});
