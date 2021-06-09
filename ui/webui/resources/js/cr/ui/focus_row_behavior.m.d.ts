// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FocusRow} from './focus_row.m.js';

export interface FocusRowBehavior {
  id: string|null|undefined;
  isFocused: boolean|null|undefined;
  focusRowIndex: number|null|undefined;
  lastFocused: Element|null;
  ironListTabIndex: number;
  listBlurred: boolean|null|undefined;
  focusRowIndexChanged(newIndex: number, oldIndex: number): void;
  getFocusRow(): FocusRow;
}

declare const FocusRowBehavior: object;
