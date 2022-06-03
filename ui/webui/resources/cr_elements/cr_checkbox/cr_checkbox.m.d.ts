// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrCheckboxElement extends LegacyElementMixin, HTMLElement {
  checked: boolean;
  disabled: boolean;
  ariaDescription: string|null|undefined;
  tabIndex: number;
  focus(): void;
  getFocusableElement(): Element;
}

export {CrCheckboxElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-checkbox': CrCheckboxElement;
  }
}
