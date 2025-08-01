// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFileCarouselElement} from './file_carousel.js';

export function getHtml(this: ComposeboxFileCarouselElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.files.map((item) => {
  return html`
    <ntp-composebox-file-thumbnail .file="${item}">
    </ntp-composebox-file-thumbnail>`;
})}
<!--_html_template_end_-->`;
  // clang-format on
}
