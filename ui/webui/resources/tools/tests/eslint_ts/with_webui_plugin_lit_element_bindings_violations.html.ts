// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-expressions
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DummyElement} from './with_webui_plugin_lit_element_bindings_violations.js';

export function getHtml(this: DummyElement) {
  return html`
<cr-input type=number value="${this.value}" aria-label="${this.label}"
    aria-description="${this.description}"
    ?disabled="${this.disabled}" error-message="${this.errorMessage}"
    min="${this.limits.min}" max="${this.limits.max}"
    ?invalid="${this.errorMessage}">
</cr-input>
`;
}
