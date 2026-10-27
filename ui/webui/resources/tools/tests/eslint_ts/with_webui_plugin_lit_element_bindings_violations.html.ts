// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-expressions
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {BindingsViolationsElement} from './with_webui_plugin_lit_element_bindings_violations.js';

export function getHtml(this: BindingsViolationsElement) {
  // clang-format off
  return html`
<cr-input type=number value="${this.value}" aria-label="${this.label}"
    aria-description="${this.description}"
    ?disabled="${this.disabled}" error-message="${this.errorMessage}"
    min="${this.limits.min}" max="${this.limits.max}"
    ?invalid="${this.errorMessage}" ?readonly="${true}"
    ?hidden="${false}" ?some-multi-word-attr="${false}"
    some-array="${this.someArrayProp}">
</cr-input>
<hello-world-child .fooString="${this.value}" .fooNumber="${this.label}"
    .fooArray="${this.getErrorMessage()}"
    .fooBoolean="${this.getLabels()}"
    .fooObject="${this.disabled ? 'A' : 'B'}"
    .mixinString="${this.disabled}"
    .nonExistentProperty="${this.value}">
</hello-world-child>
<hello-world-child .mixinString="${this.label}"></hello-world-child>
<div ?hidden="${this.getErrorMessage()}" aria-label="${this.getLabels()}">
</div>
<button ?disabled="${this.buttonDisabled}"></button>
<hello-world-child .fooString="${this.value ? 'value' : nothing}">
</hello-world-child>
<div .innerHTML="${this.trustedHtml}" .style="${this.errorMessage}"></div>
<div .innerHTML="${this.trustedHtmlOrString}"></div>
<div @click="${this.onClick}"></div>
<div @click="${this.onClick_}"></div>
<!-- ?disabled disabled="true" .fooString="bar" -->
<hello-world-child ?fooBoolean="${this.disabled}" ?is-collapsible
    ?foo-boolean="false" foo-boolean="true" foo-string="true"
    fooNumber="3" fooString="${this.label}"
    .fooString="bar" .fooBoolean=" ${this.disabled}"
    .fooNumber="${this.limits.min}px">
</hello-world-child>
<hello-world-child .fooString="prefix ${this.limits.min}"></hello-world-child>
<button disabled="true"></button>
<div aria-hidden="true" draggable="true" .style="height: ${this.limits.min}px">
</div>
<svg viewBox="0 0 24 24"></svg>
`;
  // clang-format on
}
