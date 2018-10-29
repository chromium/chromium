// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-searchable-drop-down' implements a search box with a
 * suggestions drop down.
 *
 * If the update-value-on-input flag is set, value will be set to whatever is
 * in the input box. Otherwise, value will only be set when an element in items
 * is clicked.
 */
Polymer({
  is: 'cr-searchable-drop-down',

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    placeholder: String,

    /** @type {!Array<string>} */
    items: Array,

    /** @type {string} */
    value: {
      type: String,
      notify: true,
    },

    /** @private {string} */
    searchTerm_: String,

    /** @type {string} */
    label: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    updateValueOnInput: Boolean,
  },

  /** @private */
  onClick_: function() {
    this.$$('iron-dropdown').open();
  },

  /** @private */
  onInput_: function() {
    this.searchTerm_ = this.$.search.value;

    if (this.updateValueOnInput) {
      this.value = this.$.search.value;
    }
  },

  /**
   * @param {{model:Object}} event
   * @private
   */
  onSelect_: function(event) {
    this.$$('iron-dropdown').close();

    this.value = event.model.item;
    this.searchTerm_ = '';
  },

  /** @private */
  filterItems_: function(searchTerm) {
    if (!searchTerm)
      return null;
    return function(item) {
      return item.toLowerCase().includes(searchTerm.toLowerCase());
    };
  },
});