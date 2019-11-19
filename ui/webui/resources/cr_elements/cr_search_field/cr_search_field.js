// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldBehavior.
 */

Polymer({
  is: 'cr-search-field',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
    },
  },

  /** @return {!CrInputElement} */
  getSearchInput: function() {
    return /** @type {!CrInputElement} */ (this.$.searchInput);
  },

  /** @private */
  onTapClear_: function() {
    this.setValue('');
    setTimeout(() => {
      this.$.searchInput.focus();
    });
  },
});
