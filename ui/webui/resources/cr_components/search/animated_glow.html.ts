// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchAnimatedGlowElement} from './animated_glow.js';
import {GlowAnimationState} from './constants.js';

export function getHtml(this: SearchAnimatedGlowElement) {
  /*
   * Note: this does not include ::before and
   * ::after notes. See .css file comments for more details on impl.
   * - DragDropPlaceholder for drag and drop text overlay.
   * - Gradient outer glow for so glow appears
   * outside the searchbox for expanding animation.
   * - DoubleGradient is a copy of Gradient for more intense, saturated glow
   * to appear through the frosted glass for background for drag and
   * drop animation.
   * - DoubleGradientMask is a stencil that sits on top of DoubleGradient.
   * It has a solid background but is inset from the edge, creating a
   * transparent border that reveals the gradient animation of DoubleGradient,
   * creating an opaque snake effect.
   * - Gradient is the gradient border and inner glow, depending
   * on the Background styling.
   * - Background and its ::before apply a frosted glass effect in drag and
   * drop mode, and act as overlay to help create the gradient border
   * and background colorin composebox.
   * - Audio wave provides the voice animation to show browser is listening
   */

  // clang-format off
  return html`<!--_html_template_start_-->
    <div id="dragDropPlaceholder">${this.dragDropPlaceholder}</div>
    <div class="gradient gradient-outer-glow"></div>
    <div class="double-gradient"></div>
    <div class="double-gradient-mask"></div>
    <div class="gradient"></div>
    <div class="background"
        part="composebox-background">
    </div>
    ${this.requiresVoice ? html`
      <audio-wave
          ?is-listening="${this.animationState === GlowAnimationState.LISTENING}"
          .transcript="${this.transcript}"
          .receivedSpeech="${this.receivedSpeech}">
      </audio-wave>
    ` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
