// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from '../cr_input/cr_input.m.js';

import {CrSearchFieldBehavior} from './cr_search_field_behavior.js';

interface CrSearchFieldElement extends CrSearchFieldBehavior, HTMLElement {
  $: {
    clearSearch: HTMLElement,
    searchInput: CrInputElement,
  };
  autofocus: boolean;
}

export {CrSearchFieldElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-search-field': CrSearchFieldElement;
  }
}
