// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrToolbarSearchFieldElement} from './cr_toolbar_search_field.js';

interface CrToolbarElement extends HTMLElement {
  alwaysShowLogo: boolean;
  autofocus: boolean;
  clearLabel: string;
  menuLabel: string;
  narrow: boolean;
  narrowThreshold: number;
  pageName: string;
  searchPrompt: string;
  showMenu: boolean;
  showSearch: boolean;
  spinnerActive: string;

  focusMenuButton(): void;
  getSearchField(): CrToolbarSearchFieldElement;
  isMenuFocused(): boolean;
}

export {CrToolbarElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar': CrToolbarElement;
  }
}
