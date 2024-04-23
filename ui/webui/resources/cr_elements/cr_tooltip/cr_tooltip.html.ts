// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTooltipElement} from './cr_tooltip.js';

export function getHtml(this: CrTooltipElement) {
  return html`
    <div id="tooltip" hidden part="tooltip">
      <slot></slot>
    </div>`;
}
