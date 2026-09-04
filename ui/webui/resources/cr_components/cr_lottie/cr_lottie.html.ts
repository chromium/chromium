// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrLottieElement} from './cr_lottie.js';

export function getHtml(this: CrLottieElement) {
  return html`<canvas id="canvas" ?hidden="${this.hidden}"></canvas>`;
}
