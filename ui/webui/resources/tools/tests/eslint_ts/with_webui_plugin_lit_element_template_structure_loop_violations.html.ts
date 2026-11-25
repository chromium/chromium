// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, repeat} from '//resources/lit/v3_0/lit.rollup.js';

import {asyncMap} from './with_webui_plugin_lit_element_template_structure_loop_violations.js';
import type {LoopVariablesDummyElement} from './with_webui_plugin_lit_element_template_structure_loop_violations.js';

export function getHtml(this: LoopVariablesDummyElement) {
  return html`
    <!-- Valid non-nested loops -->
    ${this.items.map(item => html`<div>${item.name}</div>`)}
    ${this.items.map(
        (item, index) => html`<div>${index}: ${item.name}</div>`)}
    ${this.items.map((_item, index) => html`<div>${index}</div>`)}
    ${this.items.map((_, _index) => html`<div>static</div>`)}
    ${this.items.map(() => html`<div>empty params</div>`)}
    ${this.items.map(({id, name}) => html`<div>${id}: ${name}</div>`)}
    ${this.pairs.map(
        ([key, value], index) => html`<div>${index}: ${key}: ${value}</div>`)}
    ${repeat(
        this.items, item => item.id,
        (item, index) => html`<div>${index}: ${item.name}</div>`)}
    ${repeat(
        this.items,
        (item, index) => html`<div>${index}: ${item.name}</div>`)}
    ${this.items.map(function(item) {
      return html`<div>${item.name}</div>`;
    })}
    ${this.items.map(labelItem => labelItem.name).join(', ')}
    ${asyncMap(
        this.items,
        (item, index) => html`<div>${index}: ${item.name}</div>`, 10)}

    <!-- Valid nested loops -->
    ${this.groups.map((group, groupIndex) => html`
      <div>
        ${group.name} (${groupIndex})
        ${group.children.map(
            (child, childIndex) =>
                html`<span>${childIndex}: ${child.label}</span>`)}
      </div>
    `)}
    ${repeat(this.groups, group => group.id, group => html`
      <div>
        ${group.children.map(item => html`<span>${item.label}</span>`)}
      </div>
    `)}
    ${asyncMap(this.groups, (item, _index) => html`
      <div>
        ${item.children.map(subItem => html`<span>${subItem.label}</span>`)}
      </div>
    `, 10)}
    ${this.groups.map(({children}) => html`
      <div>
        ${children.map(nestedDestructured =>
            html`<span>${nestedDestructured.label}</span>`)}
      </div>
    `)}
    ${this.groups.map(() => html`
      <div>
        ${this.items.map(
            nestedEmpty => html`<span>${nestedEmpty.name}</span>`)}
      </div>
    `)}
    ${this.groups.map((outerWithUnusedIndex, _index) => html`
      <div>
        ${outerWithUnusedIndex.children.map(
            (innerWithUnusedIndex, _index) =>
                html`<span>${innerWithUnusedIndex.label}</span>`)}
      </div>
    `)}

    <!-- Invalid non-nested loop variable names -->
    ${this.items.map(entry => html`<div>${entry.name}</div>`)}
    ${this.items.map((item, idx) => html`<div>${idx}: ${item.name}</div>`)}
    ${this.items.map((_entry, _i) => html`<div>bad underscore</div>`)}
    ${this.items.map(function(badFnItem) {
      return html`<div>${badFnItem.name}</div>`;
    })}
    ${this.groups.map(outerWithTextMap => html`
      <div>
        ${outerWithTextMap.children.map(c => c.label).join(', ')}
      </div>
    `)}
    ${repeat(
        this.items, row => row.id,
        (row, i) => html`<div>${i}: ${row.name}</div>`)}
    ${asyncMap(
        this.items,
        (badAsyncItem, badAsyncIdx) =>
            html`<div>${badAsyncIdx}: ${badAsyncItem.name}</div>`,
        10)}

    <!-- Invalid shadowed variable in nested loop -->
    ${this.groups.map((item, index) => html`
      <div>
        ${index}: ${item.children.map(
            (item, index) => html`<span>${index}: ${item.label}</span>`)}
      </div>
    `)}
    ${asyncMap(this.groups, (asyncOuter, _index) => html`
      <div>
        ${asyncOuter.children.map(
            asyncOuter => html`<span>${asyncOuter.label}</span>`)}
      </div>
    `, 10)}
  `;
}
