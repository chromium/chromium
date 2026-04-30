// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RecordingWaveElement} from './recording_wave.js';
import {BAR_GAP} from './recording_wave.js';

export function getHtml(this: RecordingWaveElement) {
  //clang-format off
  return html`<!--_html_template_start_-->
    <div class="wave-wrapper">
      <div
        id="barsContainer"
        style="gap: ${BAR_GAP}px">
      </div>
    </div>
  <!--_html_template_end_-->`;
  //clang-format on
}
