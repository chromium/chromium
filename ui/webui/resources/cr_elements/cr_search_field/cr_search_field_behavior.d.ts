// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface CrSearchFieldBehaviorInterface {
  label: string;
  clearLabel: string;
  hasSearchText: boolean;
  getSearchInput(): HTMLInputElement;
  getValue(): string;
  setValue(value: string, noEvent?: boolean): void;
  onSearchTermSearch(): void;
  onSearchTermInput(): void;
}

export {CrSearchFieldBehavior};

interface CrSearchFieldBehavior extends CrSearchFieldBehaviorInterface {}

declare const CrSearchFieldBehavior: object;
