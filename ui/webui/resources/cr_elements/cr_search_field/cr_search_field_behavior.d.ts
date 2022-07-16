// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from '../cr_input/cr_input.m.js';

export interface CrSearchFieldBehavior {
  label: string;
  clearLabel: string;
  hasSearchText: boolean;
  getSearchInput(): HTMLInputElement|CrInputElement;
  getValue(): string;
  setValue(value: string, noEvent?: boolean): void;
  onSearchTermSearch(): void;
  onSearchTermInput(): void;
}

declare const CrSearchFieldBehavior: object;
