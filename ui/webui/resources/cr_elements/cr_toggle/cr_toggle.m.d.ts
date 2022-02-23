// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export const MOVE_THRESHOLD_PX: number;

interface CrToggleElement extends LegacyElementMixin, HTMLElement {
  checked: boolean;
  dark: boolean;
  disabled: boolean;
}

export {CrToggleElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toggle': CrToggleElement;
  }
}
