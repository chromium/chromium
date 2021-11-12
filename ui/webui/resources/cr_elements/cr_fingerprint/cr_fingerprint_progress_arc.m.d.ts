// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

interface CrFingerprintProgressArcElement extends LegacyElementMixin,
                                                  HTMLElement {
  setProgress(
      prevPercentComplete: number, currPercentComplete: number,
      isComplete: boolean): void;
  reset(): void;
  isComplete(): boolean;
}

export {CrFingerprintProgressArcElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-fingerprint-progress-arc': CrFingerprintProgressArcElement;
  }
}
