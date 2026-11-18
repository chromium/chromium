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
    .fooNumber="${this.limits.min}px"
    foo-number="${this.disabled}" foo-boolean="${this.disabled}"
    foo-array="${this.label}" foo-object="${this.label}">
</hello-world-child>
<hello-world-child .fooString="prefix > ${this.limits.min}"
    foo-number="${this.limits.min}" foo-string="${this.limits.min}"
    .afterStaticGt="${this.label}">
</hello-world-child>
<hello-world-child .fooString="${this.label} > ${this.limits.min}"
    .afterDynamicGt="${this.label}">
</hello-world-child>
<input readonly="${this.disabled}" tabindex="${this.label}">
<button disabled="true"></button>
<div aria-hidden="true" draggable="true" .style="height: ${this.limits.min}px"
    style="${this.errorMessage}" part="${this.errorMessage}">
</div>
<iframe sandbox="${this.errorMessage}"></iframe>
<div title="a > ${this.label}">Example: foo="${this.limits}"</div>
<div title="a <b">Example: bar="${this.limits}"</div>
<svg viewBox="0 0 24 24"></svg>
<hello-world-child .protectedProp="${this.label}"
    protected-attr="a > ${this.label}"
    ?protected-bool="${this.disabled}"
    .privateProp_="${this.label}"
    private-attr_="${this.getLabels()}"
    ?private-bool_="${false}">
</hello-world-child>
`;
  // clang-format on
}
