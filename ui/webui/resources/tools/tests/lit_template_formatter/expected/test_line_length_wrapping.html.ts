// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  <!-- Case 1: Single-line text element where opening tag + text + end tag > 80 chars -->
  <span class="dummy-very-long-class-name-to-exceed-limit"
      ?hidden="${this.someCondition_}">*</span>

  <!-- Case 2: Single-line text element that fits within 80 chars -->
  <span class="short-msg" ?hidden="${this.isShort}">Hello</span>

  <!-- Case 3: Element with text child with a newline -->
  <div id="dummy-description" class="dummy-class">
    $i18n{dummyText}
  </div>

  <!-- Case 4: Element with multiline empty/whitespace child (closing tag on next line) -->
  <dummy-input aria-label="${this.inputLabel}"
      ?disabled="${!this.canPerformAction}" id="dummyInput" length="6"
      .value="${this.inputValue}" @value-changed="${this.onValueChanged}">
  </dummy-input>

  <!-- Case 5: Long attribute expression breaking across lines -->
  <div class="horizontal-row" id="dummySection"
      ?hidden="${this.dummyStateValue !== '2' || this.dummyItems.length === 0}">
    <div>$i18n{dummyItems}</div>
  </div>

  <!-- Case 6: Multiline ternary expression in attribute -->
  <cr-icon
      icon="${
          this.someConditionEnabled_ ? 'my-dummy-icons:first-icon' :
                                       'my-dummy-icons:second-icon'}">
  </cr-icon>

  <!-- Case 7: Multiline tag with text child where total length exceeds limit -->
  <dummy-button id="${this.buttonId}" class="${this.buttonClass}"
      aria-label="${this.buttonLabel}"
      @click="${this.onClick}">${this.buttonText}</dummy-button>
</div>
<!--_html_template_end_-->`;
}
