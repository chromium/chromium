// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  // clang-format off
  return html`
<div class="container" ?disabled="${this.disabled}">
    <h1>${this.title}</h1>
  <span>Test with bad indent and expr</span>
</div>
`;
  // clang-format on
}
