// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {GenericListElement} from './with_webui_plugin_lit_element_bindings_templatized_dom_nodes_child.js';
import type {TemplatizedDomNodesViolationsElement} from './with_webui_plugin_lit_element_bindings_templatized_dom_nodes_violations.js';

export interface TemplatizedDomNodes {
  'number-list': GenericListElement<number>;
  'string-list': GenericListElement<string>;
}

export function getHtml(this: TemplatizedDomNodesViolationsElement) {
  // clang-format off
  return html`
<generic-list id="number-list" .items="${this.numbers}"
    .selectedItem="${this.selectedNumber}">
</generic-list>
<generic-list id="string-list" .items="${this.numbers}"
    .selectedItem="${this.selectedNumber}">
</generic-list>
`;
  // clang-format on
}
