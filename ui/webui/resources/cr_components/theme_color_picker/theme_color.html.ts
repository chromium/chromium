// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeColorElement} from './theme_color.js';

export function getHtml(this: ThemeColorElement) {
  return html`
<cr-theme-color-check-mark-wrapper .checked="${this.checked}">
  <svg viewBox="0 0 50 50" xmlns="http://www.w3.org/2000/svg"
      xmlns:xlink="http://www.w3.org/1999/xlink">
    <rect id="foreground" x="0" y="0" width="50" height="50">
    </rect>
    <rect id="background" x="0" y="25" width="50" height="25">
    </rect>
    <rect id="base" x="25" y="25" width="25" height="25">
    </rect>
  </svg>
</cr-theme-color-check-mark-wrapper>`;
}
