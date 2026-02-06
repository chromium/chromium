// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/inline-event-handler

import type {MyDummyElement} from './with_webui_plugin_web_component_missing_deps_violations.html.js';

export function getHtml(this: MyDummyElement) {
  // clang-format off
  return html`
<cr-icon-button @click="${(e: Event) => this.onIconClick(e)}"></cr-icon-button>
<cr-input id="input" @input="${e => this.inputData_ = e.data}">
</cr-input>
<div id="animation" class="my-special-animation-class"
   @animationend="${(e: AnimationEvent) => {
      this.onAnimationEnd_(e, 'hello');
    }}">
</div>
<select class="md-select" @change="${this.onSelectChange}">
  ${this.myArray.map(item => html`
    <option value="${item.id}">${item.name}</option>
  `)}
</select>
${this.myArray.map(item => html`
  <cr-button @focus="${() => this.onItemFocus(item)}"
      @blur="${this.onItemBlur}">
    ${item.name}
  </cr-button>
`)}
`;
  // clang-format on
}
