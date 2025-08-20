// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icons.css.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxDropdownElement} from './composebox_dropdown.js';

export function getHtml(this: ComposeboxDropdownElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
  ${this.result?.matches.map((item) => html`
    <ntp-composebox-match tabindex="0" role="option" .match="${item}">
    </ntp-composebox-match>
  `)}
  <!--_html_template_end_-->`;
  // clang-format on
}
