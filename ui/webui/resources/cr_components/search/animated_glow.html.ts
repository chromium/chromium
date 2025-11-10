/**
 * Copyright 2025 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchAnimatedGlowElement} from './animated_glow.js';

export function getHtml(this: SearchAnimatedGlowElement) {
  // TODO(crbug.com/454730356): replace
  // drop string with translatable string.

  /**
  Note: this does not include ::before and ::after notes.
  See .css file comments for more details on implementation.
  * DragDropPlaceholder for drag and drop text overlay.
  * Gradient outer glow for so glow appears
  outside the searchbox for expanding animation.
  * DoubleGradient is a copy of Gradient for more intense, saturated glow
  to appear through the frosted glass for background for drag and
  drop animation.
  * Gradient is the gradient border and inner glow, depending
  on the Background styling.
  * Background is to apply a frosted glass effect in drag and drop mode,
  and to act as an overlay to help create the gradient border.
  */

  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="dragDropPlaceholder"> Drop your file here </div>
    <div class="gradient gradient-outer-glow"></div>
    <div class="double-gradient"> </div>
    <div class="gradient"></div>
    <div class="background"></div>
  <!--_html_template_end_-->`;
  // clang-format on
}
