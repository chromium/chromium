// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLottieElement} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export const FINGERPRINT_SCANNED_ICON_DARK: string;
export const FINGERPRINT_SCANNED_ICON_LIGHT: string;
export const FINGERPRINT_CHECK_DARK_URL: string;
export const FINGERPRINT_CHECK_LIGHT_URL: string;
export const PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK: string;
export const PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT: string;
export const PROGRESS_CIRCLE_FILL_COLOR_DARK: string;
export const PROGRESS_CIRCLE_FILL_COLOR_LIGHT: string;

interface CrFingerprintProgressArcElement extends LegacyElementMixin,
                                                  HTMLElement {
  circleRadius: number;

  reset(): void;
  setProgress(
      prevPercentComplete: number, currPercentComplete: number,
      isComplete: boolean): void;
  setPlay(shouldPlay: boolean): void;
  isComplete(): boolean;
  $: {
    canvas: HTMLCanvasElement,
    fingerprintScanned: IronIconElement,
    scanningAnimation: CrLottieElement,
  };
}

export {CrFingerprintProgressArcElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-fingerprint-progress-arc': CrFingerprintProgressArcElement;
  }
}
