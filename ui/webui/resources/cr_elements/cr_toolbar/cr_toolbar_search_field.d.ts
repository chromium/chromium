// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

import {CrSearchFieldBehavior} from '../cr_search_field/cr_search_field_behavior.js';

interface CrToolbarSearchFieldElement extends CrSearchFieldBehavior,
                                              LegacyElementMixin, HTMLElement {
  narrow: boolean;
  showingSearch: boolean;
  autofocus: boolean;
  label: string;
  clearLabel: string;
  spinnerActive: boolean;

  getSearchInput(): HTMLInputElement;
  isSearchFocused(): boolean;
  showAndFocus(): void;
  onSearchTermInput(): void;
}

export {CrToolbarSearchFieldElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-search-field': CrToolbarSearchFieldElement;
  }
}
