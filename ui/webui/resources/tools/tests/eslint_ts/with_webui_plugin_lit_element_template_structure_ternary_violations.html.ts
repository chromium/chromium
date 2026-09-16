// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TernaryDummyElement} from './with_webui_plugin_lit_element_template_structure_ternary_violations.js';

export function getHtml(this: TernaryDummyElement) {
  return html`
    ${this.condition1 ? '' : html`<div>Inverted</div>`}
    ${this.condition1 ? html`<div>First</div>` : this.condition2 ? html`<div>Second</div>` : ''}
  `;
}
