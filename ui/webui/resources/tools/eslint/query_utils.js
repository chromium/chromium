// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import path from 'node:path';

import esquery from '../../../../../third_party/node/node_modules/esquery/dist/esquery.esm.min.js';

// NOTE: Using `\u002F` instead of a forward slash, to workaround for
// https://github.com/eslint/eslint/issues/16555,
// https://eslint.org/docs/latest/extend/selectors#known-issues
// where forward slashes are not properly escaped in regular expressions
// appearing in AST selectors.
export const POLYMER_IMPORT_REGEX = [
  'resources',
  'polymer',
  'v3_0',
  'polymer',
  'polymer_bundled.min.js$',
].join('\\u002F');

export const LIT_IMPORT_REGEX =
    ['resources', 'lit', 'v3_0', 'lit.rollup.js$'].join('\\u002F');

export const CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR =
    'CallExpression[callee.name=/Mixin(Lit)?$/][arguments.0.name="CrLitElement"]';

export function isCrLitElementSubclass(node, programNode) {
  assert.ok(node.type === 'ClassDeclaration');

  if (!node.superClass) {
    return false;
  }

  if (node.superClass.type === 'Identifier') {
    if (node.superClass.name === 'CrLitElement') {
      // Case1: 'MyElement extends CrLitElement {...}'
      return true;
    }

    // Case2:
    // const MyElementBase = SomeMixin(CrLitElement);
    // MyElement extends MyElementBase {...}'
    const baseClassSelector = esquery.parse(
        `Program > VariableDeclaration > VariableDeclarator[id.name="${
            node.superClass.name}"] ${CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR}`);
    const matchingNodes = esquery.match(programNode, baseClassSelector);
    return matchingNodes.length > 0;
  }

  if (node.superClass.type === 'CallExpression') {
    // Case3: 'MyElement extends SomeMixin(SomeOtherMixin(CrLitElement)) {...}'
    const selector = esquery.parse(CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR);
    const matchingNodes = esquery.match(node.superClass, selector);
    return matchingNodes.length > 0;
  }

  return false;
}

export function dashCaseToCamelCase(string) {
  return string.replace(/-([a-z])/g, group => group[1].toUpperCase());
}

// Extracts information about the imported element class from a Lit element
// template file. Returns an object containing 2 properties:
// type: The type of the "this" parameter in the getHtml() method.
// fileName: The name of the file the class type is imported from.
export function extractClassImport(node, programNode) {
  assert.ok(node.type === 'FunctionDeclaration' && node.id.name === 'getHtml');
  const paramSelector = esquery.parse('Identifier[name="this"]');
  const matchingNodes = esquery.match(node, paramSelector);
  if (matchingNodes.length === 0) {
    // Case where there is no "this" parameter for the getHtml() method
    return {
      type: '',
      fileName: '',
    };
  }

  const type = matchingNodes[0].typeAnnotation.typeAnnotation.typeName.name;
  // Find the URL of the import that imports the class.
  const importNodeQuery = esquery.parse('ImportDeclaration');
  const importNodes = esquery.match(programNode, importNodeQuery);

  const classImport = importNodes
                          .find(importNode => {
                            return importNode.specifiers.some(specifier => {
                              return specifier.local.name === type;
                            });
                          })
                          .source.value;
  return {
    type: type,
    fileName: path.basename(classImport).replace('.js', '.ts'),
  };
}
