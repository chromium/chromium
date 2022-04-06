// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IronDropdownElement} from 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';

import {CrInputElement} from '../cr_input/cr_input.m.js';

interface CrSearchableDropDownElement extends HTMLElement {
  autofocus: boolean;
  readonly: boolean;
  errorMessageAllowed: boolean;
  errorMessage: string;
  loadingMessage: string;
  placeholder: string;
  invalid: boolean;
  items: string[];
  value: string;
  label: string;
  updateValueOnInput: boolean;
  showLoading: boolean;

  $: {
    search: CrInputElement,
    dropdown: IronDropdownElement,
  };
}

export {CrSearchableDropDownElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchable-drop-down': CrSearchableDropDownElement;
  }
}
