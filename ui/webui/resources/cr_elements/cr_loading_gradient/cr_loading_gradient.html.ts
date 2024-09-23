// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrLoadingGradientElement} from './cr_loading_gradient.js';

export function getHtml(this: CrLoadingGradientElement) {
  return html`
<div id="gradient"></div>
<slot @slotchange="${this.onSlotchange_}"></slot>`;
}
