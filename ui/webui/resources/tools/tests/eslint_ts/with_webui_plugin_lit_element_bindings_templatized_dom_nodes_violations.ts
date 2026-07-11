// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './with_webui_plugin_lit_element_bindings_templatized_dom_nodes_child.js';

import {CrLitElement} from '/resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './with_webui_plugin_lit_element_bindings_templatized_dom_nodes_violations.html.js';

export class TemplatizedDomNodesViolationsElement extends CrLitElement {
  static get is() {
    return 'templatized-dom-nodes-violations';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      numbers: {type: Array},
      strings: {type: Array},
      selectedNumber: {type: Number},
      selectedString: {type: String},
    };
  }

  accessor numbers: number[] = [1, 2, 3];
  accessor strings: string[] = ['a', 'b', 'c'];
  accessor selectedNumber: number = 1;
  accessor selectedString: string = 'a';
}

declare global {
  interface HTMLElementTagNameMap {
    'templatized-dom-nodes-violations': TemplatizedDomNodesViolationsElement;
  }
}

customElements.define(
    TemplatizedDomNodesViolationsElement.is,
    TemplatizedDomNodesViolationsElement);
