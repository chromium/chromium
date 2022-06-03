// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrDrawerElement extends LegacyElementMixin, HTMLElement {
  heading: string|null|undefined;
  align: string|null|undefined;
  iconName: string|null|undefined;
  iconTitle: string|null|undefined;
  open: any;
  toggle(): void;
  openDrawer(): void;
  cancel(): void;
  close(): void;
  wasCanceled(): boolean;
}

export {CrDrawerElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-drawer': CrDrawerElement;
  }
}
