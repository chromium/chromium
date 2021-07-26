// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface CrToastManagerElement extends HTMLElement {
  duration: number;
  isToastOpen: boolean;
  slottedHidden: boolean;
  show(label: string, hideSlotted?: boolean): void;
  showForStringPieces(
      pieces: Array<{value: string, collapsible: boolean}>,
      hideSlotted?: boolean): void;
  hide(): void;
}

export {CrToastManagerElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toast-manager': CrToastManagerElement;
  }
}

export function getToastManager(): CrToastManagerElement;
