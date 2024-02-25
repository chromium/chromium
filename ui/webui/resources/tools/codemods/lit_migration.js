// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const path = require('node:path');

const POLYMER_IMPORT =
    `import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';`;
const LIT_IMPORT =
    `import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';`;

const OLD_GET_TEMPLATE = `static get template() {
    return getTemplate();
  }`;

const NEW_GET_TEMPLATE = `static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }`;

function getOldTemplateImport(basename) {
  return `import {getTemplate} from './${basename}.html.js';`;
}

function getNewTemplateImport(basename) {
  return `import {getCss} from './${basename}.css.js';
import {getHtml} from './${basename}.html.js';`;
}

module.exports = function transformer(file, api) {
  // First perform easy regex based transformations that don't require a  parser
  // and a full AST representation.

  let source = file.source;

  // Step 1: Replace PolymerElement import.
  source = source.replace(POLYMER_IMPORT, LIT_IMPORT);

  // Step 2: Replace getTemplate import.
  const basename = path.basename(file.path, '.ts');
  const OLD_TEMPLATE_IMPORT = getOldTemplateImport(basename);
  const NEW_TEMPLATE_IMPORT = getNewTemplateImport(basename);
  source = source.replace(OLD_TEMPLATE_IMPORT, NEW_TEMPLATE_IMPORT);

  // Step 3: Replace extends PolymerElement
  source = source.replace('extends PolymerElement', 'extends CrLitElement');

  // Step 4: Replace 'getTemplate()' calls.
  source = source.replace(OLD_GET_TEMPLATE, NEW_GET_TEMPLATE);

  // Step 5: Replace 'static get properties() {...}'
  source = source.replace(
      'static get properties() {', 'static override get properties() {');

  // Step6: Update 'reflectToAttribute' attribute
  source = source.replaceAll('reflectToAttribute: true', 'reflect: true');

  // Step7: Update ready() callbacks.
  source = source.replace('override ready() {', 'override firstUpdated() {');
  source = source.replace(/\s+super.ready\(\);\n/, '');

  // Secondly, perform more complex transformations that require a parser and a
  // proper AST representation.

  const j = api.jscodeshift;
  const root = j(source);

  // Step8: Replace "foo: <Type>" shorthand with "foo: {type: <Type>}"
  root.find(j.Function, {key: {name: 'properties'}})
      .find(j.ObjectExpression)
      .forEach(p => {
        p.value.properties.forEach(property => {
          // Ignore properties that already use the non-shorthand Polymer
          // syntax.
          if (p.parentPath.value.type !== 'ReturnStatement') {
            return;
          }
          if (property.value.type === 'ObjectExpression') {
            return;
          }

          // Create replacement node.
          const expression = j.objectExpression([
            j.property(
                'init', j.identifier('type'),
                j.identifier(property.value.name)),
          ]);

          // Add replacement node to the tree.
          property.value = expression;
        });
      });

  // TODO(crbug.com/40943652): Make more AST transformations here.

  const outputOptions = {quote: 'single'};
  return root.toSource(outputOptions);
};
