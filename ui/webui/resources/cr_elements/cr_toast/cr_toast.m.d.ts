// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrToastElement extends LegacyElementMixin, HTMLElement {
  duration: number|null|undefined;
  readonly open: boolean|null|undefined;
  show(): void;
  hide(): void;
}

export {CrToastElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toast': CrToastElement;
  }
}
