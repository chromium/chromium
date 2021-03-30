// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_icon_button/cr_icon_button.m.js';
import '../cr_icons_css.m.js';
import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSearchFieldBehavior} from '../cr_search_field/cr_search_field_behavior.js';

Polymer({
  is: 'cr-toolbar-search-field',

  _template: html`{__html_template__}`,

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

    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
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
  getSearchInput() {
    return /** @type {!HTMLInputElement} */ (this.$.searchInput);
  },

  /** @return {boolean} */
  isSearchFocused() {
    return this.searchFocused_;
  },

  showAndFocus() {
    this.showingSearch = true;
    this.focus_();
  },

  onSearchTermInput() {
    CrSearchFieldBehavior.onSearchTermInput.call(this);
    this.showingSearch = this.hasSearchText || this.isSearchFocused();
  },

  /** @private */
  onSearchIconClicked_() {
    this.fire('search-icon-clicked');
  },

  /** @private */
  focus_() {
    this.getSearchInput().focus();
  },

  /**
   * @param {boolean} narrow
   * @return {number}
   * @private
   */
  computeIconTabIndex_(narrow) {
    return narrow && !this.hasSearchText ? 0 : -1;
  },

  /**
   * @param {boolean} narrow
   * @return {string}
   * @private
   */
  computeIconAriaHidden_(narrow) {
    return Boolean(!narrow || this.hasSearchText).toString();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSpinnerShown_() {
    const showSpinner = this.spinnerActive && this.showingSearch;
    if (showSpinner) {
      this.$.spinnerTemplate.if = true;
    }
    return showSpinner;
  },

  /** @private */
  onInputFocus_() {
    this.searchFocused_ = true;
  },

  /** @private */
  onInputBlur_() {
    this.searchFocused_ = false;
    if (!this.hasSearchText) {
      this.showingSearch = false;
    }
  },

  /** @private */
  onSearchTermKeydown_(e) {
    if (e.key === 'Escape') {
      this.showingSearch = false;
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  showSearch_(e) {
    if (e.target !== this.$.clearSearch) {
      this.showingSearch = true;
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  clearSearch_(e) {
    this.setValue('');
    this.focus_();
    this.spinnerActive = false;
  },

  /**
   * @param {boolean} current
   * @param {boolean|undefined} previous
   * @private
   */
  showingSearchChanged_(current, previous) {
    // Prevent unnecessary 'search-changed' event from firing on startup.
    if (previous === undefined) {
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
