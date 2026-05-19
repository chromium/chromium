// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import {unlinkSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {dirname, join} from 'node:path';
import {after, suite, test} from 'node:test';
import {fileURLToPath} from 'node:url';

import {formatTsExpressions} from './format_expressions.js';
import {prepareHtmlAst} from './format_html.js';
import {EXPR_PREFIX, getClangFormatPath, PROP_PREFIX, TEMPLATE_PREFIX} from './html_utils.js';
import {processTemplate} from './process_lit_template_ts.js';
import {serializeHtmlAst} from './serialize_html.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const workspaceRoot = join(__dirname, '../../../../../');
const clangFormatPath = getClangFormatPath(workspaceRoot);

suite('lit_template_formatter', () => {
  let tempFile;

  after(() => {
    if (tempFile) {
      try {
        unlinkSync(tempFile);
      } catch (_e) {
      }
    }
  });

  test('processTemplate', () => {
    tempFile = join(tmpdir(), 'temp_test_file.ts');
    const fileContent = `
      import {html} from '//resources/lit/v3_0/lit.rollup.js';
      export function getHtml(this: DummyTestElement) {
        return html\`
          <div ?disabled="\${this.disabled}" .template="\${this.items.map(item => html\`<span>\${item}</span>\`)}">
          </div>
        \`;
      }
    `;
    writeFileSync(tempFile, fileContent, 'utf-8');
    const {result, map} = processTemplate(tempFile);
    const expectedResult =
        `\n          <div ?disabled="${EXPR_PREFIX}-1" .template="${
            EXPR_PREFIX}-4">\n          </div>\n        `;
    assert.strictEqual(
        result, expectedResult, 'Result HTML string should match exactly');

    const expectedMap = [
      [`${EXPR_PREFIX}-1`, {code: '${this.disabled}'}],
      [`${EXPR_PREFIX}-3`, {code: '${item}'}],
      [`${TEMPLATE_PREFIX}-2`, {code: '${this.items.map(item => html`'}],
      [`/${TEMPLATE_PREFIX}-2`, {code: '`)}'}],
      [
        `${EXPR_PREFIX}-4`,
        {
          code: `<${TEMPLATE_PREFIX}-2><span>${EXPR_PREFIX}-3</span></${
              TEMPLATE_PREFIX}-2>`,
          nested: true,
        },
      ],
    ];
    assert.deepStrictEqual(
        Array.from(map.entries()), expectedMap,
        'Placeholder map structure should match exactly');
  });

  test('prepareHtmlAst', () => {
    const template = `
      <div class="container" ?disabled="${EXPR_PREFIX}-1">
        <span>${EXPR_PREFIX}-2</span>
      </div>
    `;
    const map = new Map([
      [`${EXPR_PREFIX}-1`, {code: '${this.disabled}'}],
      [`${EXPR_PREFIX}-2`, {code: '${this.text}'}],
    ]);

    const ast = prepareHtmlAst(template, map);
    assert.ok(ast, 'AST should be constructed');
    assert.strictEqual(
        ast.childNodes.length, 2, 'Should have 2 nodes (text, div)');

    const p1 = map.get(`${EXPR_PREFIX}-1`);
    assert.strictEqual(
        p1.indent, 0, 'Should record indentation metadata for attributes');

    const p2 = map.get(`${EXPR_PREFIX}-2`);
    assert.strictEqual(
        p2.indent, 4,
        'Should record indentation metadata for child text nodes');
  });

  test('prepareHtmlAstRestrictedTags', () => {
    const template = `
      <select>
        <option>${EXPR_PREFIX}-1</option>
      </select>
    `;
    const map = new Map([
      [`${EXPR_PREFIX}-1`, {code: '${this.item}'}],
    ]);

    const ast = prepareHtmlAst(template, map);
    assert.ok(ast, 'AST for restricted tags should be constructed');

    // The select element should have been preprocessed to lit-select
    const selectNode = ast.childNodes.find(c => c.nodeName !== '#text');
    assert.strictEqual(
        selectNode.tagName, 'lit-select',
        'Select tag should be converted to lit-select in AST');
  });

  test('prepareHtmlAstProperties', () => {
    const template = `
      <div .someProp="${EXPR_PREFIX}-1"></div>
    `;
    const map = new Map([
      [`${EXPR_PREFIX}-1`, {code: '${this.value}'}],
    ]);

    const ast = prepareHtmlAst(template, map);
    assert.ok(ast, 'AST for properties should be constructed');

    // The .someProp attribute should have been replaced with a lit-prop
    // placeholder in the AST
    const divNode = ast.childNodes.find(c => c.nodeName !== '#text');
    const attrNames = divNode.attrs.map(a => a.name);
    assert.ok(
        attrNames.includes(`${PROP_PREFIX}-1`),
        `Property binding should be replaced with ${PROP_PREFIX}-1 in AST`);

    const p = map.get(`${PROP_PREFIX}-1`);
    assert.strictEqual(
        p.code, '.someProp',
        'Placeholder mapping must capture original property name .someProp');
  });

  test('formatTsExpressions', async () => {
    const map = new Map([
      [
        `${EXPR_PREFIX}-1`,
        {
          code:
              '${this.someItems.map(item => item.value).filter(item => item.active).reduce((a, b) => a + b, 0)}',
          indent: 20,
        },
      ],
    ]);

    await formatTsExpressions(
        map, clangFormatPath, join(__dirname, 'dummy.ts'));
    const p1 = map.get(`${EXPR_PREFIX}-1`);
    assert.ok(
        p1.code.includes('\n'),
        'Expression should be wrapped across lines by clang-format');
    assert.ok(
        p1.code.includes('    '), 'Wrapped lines should be indented properly');
  });

  test('serializeHtmlAst', () => {
    // Test 1: Attribute wrapping and empty alt attribute preservation
    const template = `
      <img id="logo" class="brand-logo" src="${
        EXPR_PREFIX}-1" alt="" ?disabled="${EXPR_PREFIX}-2">
    `;
    const map = new Map([
      [`${EXPR_PREFIX}-1`, {code: '${this.logoUrl}'}],
      [`${EXPR_PREFIX}-2`, {code: '${this.disabled}'}],
    ]);

    const ast = prepareHtmlAst(template, map);
    const serialized = serializeHtmlAst(ast, map, /* sortAttributes= */ false);

    const expectedSerialized = `
<img id="logo" class="brand-logo" src="\${this.logoUrl}" alt=""
    ?disabled="\${this.disabled}">`;
    assert.strictEqual(
        serialized, expectedSerialized,
        'Serialized output with wrapped attributes must match exactly');

    // Test 2: lit-template-format off/on blocks
    const blockTemplate = `
      <div>
        <!-- lit-template-format-off -->
        <span   class="weird-spacing" >No Formatting</span>
        <!-- lit-template-format-on -->
      </div>
    `;
    const blockMap = new Map();
    const blockAst = prepareHtmlAst(blockTemplate, blockMap);
    const blockSerialized = serializeHtmlAst(blockAst, blockMap, false);
    assert.ok(
        blockSerialized.includes(
            '<span   class="weird-spacing" >No Formatting</span>'),
        'lit-template-format off block must be preserved exactly');
  });
});
