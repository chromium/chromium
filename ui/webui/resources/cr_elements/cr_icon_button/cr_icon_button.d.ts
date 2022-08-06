// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrIconButtonElement extends LegacyElementMixin, HTMLElement {
  disabled: boolean;
  customTabIndex: number|null|undefined;
  ironIcon: string|null|undefined;
  hostAttributes: object|null;
}

export {CrIconButtonElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-icon-button': CrIconButtonElement;
  }
}
