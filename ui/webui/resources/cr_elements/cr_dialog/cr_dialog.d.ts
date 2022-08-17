// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrDialogElement extends HTMLElement {
  open: boolean;
  closeText: string|null|undefined;
  ignorePopstate: boolean;
  ignoreEnterKey: boolean;
  consumeKeydownEvent: boolean;
  noCancel: boolean;
  showCloseButton: boolean;
  showOnAttach: boolean;
  showModal(): void;
  cancel(): void;
  close(): void;
  setTitleAriaLabel(title: string): void;
  getNative(): HTMLDialogElement;
  focus(): void;

  $: {
    close: HTMLElement,
  };
}

export {CrDialogElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-dialog': CrDialogElement;
  }
}
