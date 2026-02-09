// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/logic-in-template-file

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MyDummyElement} from './with_webui_plugin_logic_in_template_file_violations.js';

// Extra function declaration violation, belongs in a custom element or
// inlined.
function getButtonHtml(
    this: MyDummyElement, message: string, disabled: boolean) {
  return html`
<div class="button-container">
  <cr-button @click="${this.onButtonClick_}" ?disabled="${disabled}">
    ${message}
  </cr-button>
</div>
`;
}

// Extra function declaration violation, belongs in protected method in
// class definition file.
function computeFoo(this: MyDummyElement) {
  return (this.bar + this.baz) * 100;
}

export function getHtml(this: MyDummyElement) {
  // For loop violation, indicating overly complex logic. Belongs in class
  // definition file.
  let showFatalError = false;
  const fatalErrors = [];
  for (const error of this.errors_) {
    if (error.isFatal) {
      showFatalError = true;
      fatalErrors.push(error);
    }
  }

  // Extra function declaration, which is embedded in getHtml().
  function getSpinnerDiv() {
    return html`<div id="spinner" class="spinner"></div>`;
  }

  // clang-format off
  return html`
${showFatalError ? html`
  <div class="fatal-error">Fatal Errors: ${fatalErrors.join(',')}</div>
` : html`
  <div class="title">Dummy Title</div>
  ${getButtonHtml.bind(this)()}
  <div>Percent Complete: ${computeFoo.bind(this)()}</div>
  ${getSpinnerDiv()}
`}
`;
  // clang-format on
}
