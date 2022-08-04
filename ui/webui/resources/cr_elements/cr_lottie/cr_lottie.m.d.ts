// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LegacyElementMixin} from 'chrome://resources/polymer/v3_0/polymer/lib/legacy/legacy-element-mixin.js';

export const LOTTIE_JS_URL: string;

interface CrLottieElement extends LegacyElementMixin, HTMLElement {
  animationUrl: string;
  autoplay: boolean;
  setPlay(shouldPlay: boolean): void;
  singleLoop: boolean;

  $: {
    canvas: HTMLCanvasElement,
  };
}

export {CrLottieElement};

declare global {
  interface HTMLElementTagNameMap {
    'cr-lottie': CrLottieElement;
  }
}
