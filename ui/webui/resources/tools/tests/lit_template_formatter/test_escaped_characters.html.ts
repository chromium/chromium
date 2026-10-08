// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  return html`<!--_html_template_start_-->
<div>
  <cr-input id="firstInput" pattern=".*\\S.*" .value="${this.firstName}">
  </cr-input>
  <cr-input id="secondInput" pattern=".*\\S.*" .value="${this.lastName}">
  </cr-input>
  ${this.showExtra ? html`
    <cr-input id="extraInput" pattern=".*\\d.*"></cr-input>
  ` : ''}
</div>
<!--_html_template_end_-->`;
}
