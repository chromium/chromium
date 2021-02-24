// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '../cr_icon_button/cr_icon_button.m.js';
import '../cr_icons_css.m.js';
import './cr_toolbar_search_field.m.js';
import '../hidden_style_css.m.js';
import '../icons.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

Polymer({
  is: 'cr-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    // Name to display in the toolbar, in titlecase.
    pageName: String,

    // Prompt text to display in the search field.
    searchPrompt: String,

    // Tooltip to display on the clear search button.
    clearLabel: String,

    // Tooltip to display on the menu button.
    menuLabel: String,

    // Value is proxied through to cr-toolbar-search-field. When true,
    // the search field will show a processing spinner.
    spinnerActive: Boolean,

    // Controls whether the menu button is shown at the start of the menu.
    showMenu: {type: Boolean, value: false},

    // Controls whether the search field is shown.
    showSearch: {type: Boolean, value: true},

    // Controls whether the search field is autofocused.
    autofocus: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    // True when the toolbar is displaying in narrow mode.
    narrow: {
      type: Boolean,
      reflectToAttribute: true,
      readonly: true,
      notify: true,
    },

    /**
     * The threshold at which the toolbar will change from normal to narrow
     * mode, in px.
     */
    narrowThreshold: {
      type: Number,
      value: 900,
    },

    /** @private */
    showingSearch_: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  /** @return {!CrToolbarSearchFieldElement} */
  getSearchField() {
    return /** @type {!CrToolbarSearchFieldElement} */ (this.$.search);
  },

  /** @private */
  onMenuTap_() {
    this.fire('cr-toolbar-menu-tap');
  },

  focusMenuButton() {
    requestAnimationFrame(() => {
      // Wait for next animation frame in case dom-if has not applied yet and
      // added the menu button.
      const menuButton = this.shadowRoot.querySelector('#menuButton');
      if (menuButton) {
        menuButton.focus();
      }
    });
  },

  /** @return {boolean} */
  isMenuFocused() {
    return Boolean(this.shadowRoot.activeElement) &&
        this.shadowRoot.activeElement.id === 'menuButton';
  }
});
