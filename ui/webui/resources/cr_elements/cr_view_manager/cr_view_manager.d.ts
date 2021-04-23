// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrViewManagerElement extends LegacyElementMixin, HTMLElement {
  switchView(
      newViewId: string, enterAnimation?: string,
      exitAnimation?: string): Promise<any>;
}

export {CrViewManagerElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-view-manager': CrViewManagerElement;
  }
}
