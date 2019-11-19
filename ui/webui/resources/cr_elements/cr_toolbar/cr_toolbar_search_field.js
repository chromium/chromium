// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-toolbar-search-field',

  behaviors: [CrSearchFieldBehavior],

  properties: {
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
    },

    showingSearch: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'showingSearchChanged_',
      reflectToAttribute: true
    },

    // Prompt text to display in the search field.
    label: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,

    // When true, show a loading spinner to indicate that the backend is
    // processing the search. Will only show if the search field is open.
    spinnerActive: {type: Boolean, reflectToAttribute: true},

    /** @private */
    isSpinnerShown_: {
      type: Boolean,
      computed: 'computeIsSpinnerShown_(spinnerActive, showingSearch)'
    },

    /** @private */
    searchFocused_: {type: Boolean, value: false},
  },

  listeners: {
    // Deliberately uses 'click' instead of 'tap' to fix crbug.com/624356.
    'click': 'showSearch_',
  },

  /** @return {!HTMLInputElement} */
  getSearchInput: function() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  },

  /** @return {boolean} */
  isSearchFocused: function() {
    return this.searchFocused_;
  },

  showAndFocus: function() {
    this.showingSearch = true;
    this.focus_();
  },

  onSearchTermInput: function() {
    CrSearchFieldBehavior.onSearchTermInput.call(this);
    this.showingSearch = this.hasSearchText || this.isSearchFocused();
  },

  /** @private */
  focus_: function() {
    this.getSearchInput().focus();
  },

  /**
   * @param {boolean} narrow
   * @return {number}
   * @private
   */
  computeIconTabIndex_: function(narrow) {
    return narrow && !this.hasSearchText ? 0 : -1;
  },

  /**
   * @param {boolean} narrow
   * @return {string}
   * @private
   */
  computeIconAriaHidden_: function(narrow) {
    return Boolean(!narrow || this.hasSearchText).toString();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSpinnerShown_: function() {
    const showSpinner = this.spinnerActive && this.showingSearch;
    if (showSpinner) {
      this.$.spinnerTemplate.if = true;
    }
    return showSpinner;
  },

  /** @private */
  onInputFocus_: function() {
    this.searchFocused_ = true;
  },

  /** @private */
  onInputBlur_: function() {
    this.searchFocused_ = false;
    if (!this.hasSearchText) {
      this.showingSearch = false;
    }
  },

  /** @private */
  onSearchTermKeydown_: function(e) {
    if (e.key == 'Escape') {
      this.showingSearch = false;
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  showSearch_: function(e) {
    if (e.target != this.$.clearSearch) {
      this.showingSearch = true;
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  clearSearch_: function(e) {
    this.setValue('');
    this.focus_();
    this.spinnerActive = false;
  },

  /**
   * @param {boolean} current
   * @param {boolean|undefined} previous
   * @private
   */
  showingSearchChanged_: function(current, previous) {
    // Prevent unnecessary 'search-changed' event from firing on startup.
    if (previous == undefined) {
      return;
    }

    if (this.showingSearch) {
      this.focus_();
      return;
    }

    this.setValue('');
    this.getSearchInput().blur();
  },
});
