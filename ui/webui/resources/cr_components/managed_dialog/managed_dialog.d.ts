// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface ManagedDialogElement extends HTMLElement {
  title: string;
  body: string;
}

export {ManagedDialogElement};

declare global {
  interface HTMLElementTagNameMap {
    'managed-dialog': ManagedDialogElement;
  }
}
