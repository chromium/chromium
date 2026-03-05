// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/logic-in-template-file

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

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
function computeProgress(this: MyDummyElement) {
  return Math.round(this.loadProgress * 100);
}

// Local variable violation, outside of getHtml()
const INPUT_MAX_LENGTH = 100;

export function getHtml(this: MyDummyElement) {
  // If statement violation
  if (this.data === null) {
    return nothing;
  }

  // Local variable violation
  const messagesToRender = [];
  // For loop violation, indicating overly complex logic. Belongs in class
  // definition file, or in a map() statement.
  for (const log of this.data.log) {
    messagesToRender.push(log.dateString + ': ' + log.message);
  }

  // Extra function declaration, which is embedded in getHtml().
  function getSpinnerDiv() {
    return html`<div id="spinner" class="spinner"></div>`;
  }

  // Local variable violations.
  // clang-format off
  const input = html`
      <cr-input maxlength="${INPUT_MAX_LENGTH}" @input="${this.onInput_}">
      </cr-input>`;

  let titleClass = 'title';
  if (this.fancyTitle) {
    titleClass = 'fancy-title';
  }

  return html`
<div class="${titleClass}">Dummy Title</div>
${this.loading ? html`
  <div>Percent Complete: ${computeProgress.bind(this)()}</div>
  ${getSpinnerDiv()}
` : html`
  ${messagesToRender.map(message => html`<div>${message}</div>`)}
  ${getButtonHtml.bind(this)('Reload', !this.enableReload)}
  ${input}
`}
`;
  // clang-format on
}
