// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrTabsElement extends HTMLElement {
  tabIcons: string[];
  tabNames: string[];
  selected: number;
}

export {CrTabsElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-tabs': CrTabsElement;
  }
}
