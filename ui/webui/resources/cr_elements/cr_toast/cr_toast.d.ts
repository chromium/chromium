// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrToastElement extends HTMLElement {
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
