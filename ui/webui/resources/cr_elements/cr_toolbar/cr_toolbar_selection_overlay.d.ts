// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from '../cr_button/cr_button.m.js';

interface CrToolbarSelectionOverlayElement extends HTMLElement {
  cancelLabel: string;
  deleteButton: CrButtonElement;
  deleteDisabled: boolean;
  deleteLabel: string;
  selectionLabel: string;
  show: boolean;
}

export {CrToolbarSelectionOverlayElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-toolbar-selection-overlay': CrToolbarSelectionOverlayElement;
  }
}
