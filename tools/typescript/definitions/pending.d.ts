// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some DOM element properties that are available in Chrome
// but not in TypeScript lib.d.ts since they're not generally available on all
// browsers.

// The following interface are removed from TypeScript 4.4 default
// lib.dom.d.ts.
// See https://github.com/microsoft/TypeScript-DOM-lib-generator/issues/1029
// for detail.
interface DocumentOrShadowRoot {
  getSelection(): Selection|null;
}

interface HTMLElement {
  scrollIntoViewIfNeeded(): void;
}

interface HTMLDialogElement {
  close(returnValue?: string): void;
  open: boolean;
  returnValue: string;
  showModal(): void;
}
