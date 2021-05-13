// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export interface ShowAtConfig {
  top?: number;
  left?: number;
  width?: number;
  height?: number;
  anchorAlignmentX?: number;
  anchorAlignmentY?: number;
  minX?: number;
  minY?: number;
  maxX?: number;
  maxY?: number;
  noOffset?: boolean;
}

export interface ShowAtPositionConfig {
  top: number;
  left: number;
  width?: number;
  height?: number;
  anchorAlignmentX?: number;
  anchorAlignmentY?: number;
  minX?: number;
  minY?: number;
  maxX?: number;
  maxY?: number;
}

export enum AnchorAlignment {
  BEFORE_START = -2,
  AFTER_START = -1,
  CENTER = 0,
  BEFORE_END = 1,
  AFTER_END = 2,
}

interface CrActionMenuElement extends LegacyElementMixin, HTMLElement {
  autoReposition: boolean|null|undefined;
  open: boolean|null|undefined;
  roleDescription: string|null|undefined;
  getDialog(): HTMLDialogElement;
  close(): void;
  showAt(anchorElement: Element, opt_config?: ShowAtConfig|null): void;
  showAtPosition(config: ShowAtPositionConfig): void;
}

export {CrActionMenuElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-action-menu': CrActionMenuElement;
  }
}
