// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLottieElement} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';
import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export const FINGERPRINT_TICK_DARK_URL: string;
export const FINGERPRINT_TICK_LIGHT_URL: string;

interface CrFingerprintProgressArcElement extends LegacyElementMixin,
                                                  HTMLElement {
  circleRadius: number;
  canvasCircleStrokeWidth: number;
  canvasCircleBackgroundColor: string;
  canvasCircleProgressColor: string;

  drawArc(startAngle: number, endAngle: number, color: string): void;
  drawBackgroundCircle(): void;

  setProgress(
      prevPercentComplete: number, currPercentComplete: number,
      isComplete: boolean): void;
  reset(): void;
  isComplete(): boolean;
  $: {
    canvas: HTMLCanvasElement,
    scanningAnimation: CrLottieElement,
  };
}

export {CrFingerprintProgressArcElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-fingerprint-progress-arc': CrFingerprintProgressArcElement;
  }
}
