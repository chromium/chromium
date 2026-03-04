// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for @webui-eslint/lit-element-incorrect-interface
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MyDummyElement} from './with_webui_plugin_lit_element_incorrect_interface_violations.js';

export function getHtml(this: MyDummyElement) {
  return html`
<div id="one">Hello</div>
<div id="two">World</div>
<input id="three" type="number" value="3" min="0" max="5"/>
<select class="md-select" id="four-four">
  <option value="one">one</option>
  <option value="two">two</option>
</select>
`;
}
