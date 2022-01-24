// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrSearchableDropDownElement extends LegacyElementMixin, HTMLElement {
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
}

export {CrSearchableDropDownElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchable-drop-down': CrSearchableDropDownElement;
  }
}
