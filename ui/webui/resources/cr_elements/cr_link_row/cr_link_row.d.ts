// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrLinkRowElement extends HTMLElement {
  startIcon: string;
  label: string;
  subLabel: string;
  disabled: boolean;
  external: boolean;
  usingSlottedLabel: boolean;
  roleDescription: string;
}

export {CrLinkRowElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-link-row': CrLinkRowElement;
  }
}
