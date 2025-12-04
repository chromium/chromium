// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {blurredRectUrl} from './audio_wave.js';
import type {AudioWaveElement} from './audio_wave.js';

export function getHtml(this: AudioWaveElement) {
  /*
   * Defs section:
   * - Mask added to fade (opaque to clear to opaque) thin path properly.
   * - Thin path to be the very thin white outline (like the source of the
   *   glow).
   * - clipPath to clip the glow so it does not go above wave (glows
   *   downwards).
   * - Master curve (curve + blurred set of 3 curves; lower-glow-path)
   *    modified by requestAnimationFrame() and copied by 3 other curves
   *    for varying blurs and positioning for nuanced diffused glow.
   * Use section: applies the mask to the image.
   */
  //clang-format on
  return html`<!--_html_template_start_-->
    <div id="eclipseSvgWrapper" class="eclipse-svg-wrapper">
      <svg id="eclipseSvg" class="eclipse-svg" width="100%" height="100%">
        <defs>
          <linearGradient id="fadeEnds" x1="0%" y1="0%" x2="100%" y2="0%">
            <stop offset="0%" stop-color="black" />
            <stop offset="13%" stop-color="white" />
            <stop offset="87%" stop-color="white" />
            <stop offset="100%" stop-color="black" />
          </linearGradient>

          <filter id="lowerBlur" x="-20%" y="0%" width="140%" height="400%">
            <feGaussianBlur in="SourceGraphic" stdDeviation="15" />
          </filter>

          <clipPath id="clipPath">
            <path id="clipPathShape" />
          </clipPath>

          <path id="thinPath" />
          <path id="lowerGlowPath" />

          <mask id="mask">
            <use href="#thinPath" fill="url(#fadeEnds)" />
            <g clip-path="url(#clipPath)" filter="url(#lowerBlur)">
              <use href="#lowerGlowPath"
                   fill="white" opacity=".35" transform="scale(1, 1.5)" />
              <use href="#lowerGlowPath"
                   fill="white" opacity="0.0875" transform="scale(1, 4.125)" />
              <use href="#lowerGlowPath"
                   fill="white" opacity="0.0525" transform="scale(1, 6)" />
            </g>
          </mask>
        </defs>

        <use href="#lowerGlowPath" class="eclipse-solid-fill" />
        <g mask="url(#mask)">
          <g class="blurred-rects-outer">
            <image
                href="${blurredRectUrl}"
                id="blurredRects"
                class="blurred-rects"
                transform="scale(10)"
            />
          </g>
        </g>
      </svg>
    </div>
  <!--_html_template_end_-->`;
  //clang-format off
}
