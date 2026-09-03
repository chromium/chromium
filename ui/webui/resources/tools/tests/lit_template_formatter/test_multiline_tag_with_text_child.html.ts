// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  // clang-format off
  return html`
<dummy-button id="${this.buttonId}" class="${this.buttonClass}" aria-label="${this.buttonLabel}" @click="${this.onClick}">${this.buttonText}</dummy-button>`;
  // clang-format on
}
