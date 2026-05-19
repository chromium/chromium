// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
// Disable no-restricted-syntax to allow NodeJS imports which are extensionless.
// eslint-disable-next-line no-restricted-syntax
import {readFile} from 'node:fs/promises';

import ts from '../../../../../third_party/node/node_modules/typescript/lib/typescript.js';

import {EXPR_PREFIX, FALSE_TEMPLATE_PREFIX, TEMPLATE_PREFIX} from './html_utils.js';

/**
 * Processes a Lit template string, replacing JS expressions with placeholders.
 * Uses the TypeScript compiler AST to accurately track state.
 * @param {string} filePath The file containing a template to process.
 * @return {{result: string,
 *           map: Map<string, {code: string}>,
 *           placeholder: string,
 *           restOfFile: string}}
 * |result| contains the HTML template string, or is empty if no Lit HTML
 * template was found (indicating this may not be a Lit template file).
 * |map| maps Lit expression placeholders to the original code.
 * |placeholder| is the placeholder inserted in |restOfFile| in place of the
 *     template.
 * |restOfFile| is the original file content, with the placeholder replacing
 * the HTML template string.
 */
export function processTemplate(filePath) {
  let result = '';
  const map = new Map();
  let placeholderId = 1;
  let code = '';

  // Helper to check if a node is a Lit template
  function isLitTemplate(node) {
    return ts.isTaggedTemplateExpression(node) &&
        node.tag.getText(sourceFile) === 'html';
  }

  // Unified helper to create placeholders and update map.
  // Takes an object for better readability.
  function createPlaceholder(
      {node, jsStart, closeToken, isTemplate, prefix = TEMPLATE_PREFIX}) {
    const id = placeholderId++;
    const tagName = `${prefix}-${id}`;

    const innerResult =
        isTemplate ? processNode(node) : node.getText(sourceFile);

    // Always compute jsEnd based on node.getStart().
    // If it is a template, we add +1 to include the opening backtick of the
    // template literal.
    const jsEnd = node.getStart(sourceFile) + (isTemplate ? 1 : 0);
    const jsText = code.substring(jsStart, jsEnd);

    map.set(tagName, {code: jsText});
    map.set(`/${tagName}`, {code: closeToken});

    return `<${tagName}>${innerResult}</${tagName}>`;
  }

  // Helper to find a nested template and return it directly.
  // ts.forEachChild returns the first truthy value returned by the callback.
  function findTemplate(node) {
    if (isLitTemplate(node)) {
      return node.template;
    }
    return ts.forEachChild(node, findTemplate);
  }

  function processNode(node) {
    // Early return for no substitution literals.
    // Return pure text without outer backticks.
    if (ts.isNoSubstitutionTemplateLiteral(node)) {
      return node.text;
    }

    // Early return if not a template expression
    if (!ts.isTemplateExpression(node)) {
      return '';
    }

    // Start with pure head text, without opening backtick
    let templateResult = node.head.text;

    node.templateSpans.forEach((span, index, spans) => {
      const expr = span.expression;

      const nestedTemplate = findTemplate(expr);

      if (!nestedTemplate) {
        // Case with no nested template in the expression
        const id = placeholderId++;
        const placeholder = `${EXPR_PREFIX}-${id}`;
        map.set(placeholder, {code: `\${${expr.getText(sourceFile)}}`});
        templateResult += placeholder;
      } else if (ts.isConditionalExpression(expr)) {
        // Handle conditional expressions: create placeholders for both cases'
        // templates
        const isTrueTemplate = isLitTemplate(expr.whenTrue);
        templateResult += createPlaceholder({
          node: isTrueTemplate ? expr.whenTrue.template : expr.whenTrue,
          // Subtract 2 to include the opening ${ in the placeholder mapping
          jsStart: expr.getStart(sourceFile) - 2,
          closeToken: isTrueTemplate ? '`' : '',
          isTemplate: isTrueTemplate,
        });

        const isFalseTemplate = isLitTemplate(expr.whenFalse);
        templateResult += createPlaceholder({
          node: isFalseTemplate ? expr.whenFalse.template : expr.whenFalse,
          jsStart: expr.whenTrue.getEnd(),
          closeToken: isFalseTemplate ? '`}' : '}',
          isTemplate: isFalseTemplate,
          prefix: FALSE_TEMPLATE_PREFIX,
        });
      } else {
        // General approach for other expressions containing templates (like
        // maps)
        const nestedResult = createPlaceholder({
          node: nestedTemplate,
          // Subtract 2 to include the opening ${ in the placeholder mapping
          jsStart: expr.getStart(sourceFile) - 2,
          // Include the closing backtick and the closing } in closeToken
          closeToken:
              code.substring(nestedTemplate.getEnd() - 1, expr.getEnd()) + '}',
          isTemplate: true,
        });
        // Check if this template is nested inside of a tag. If so, substitute
        // out the entire thing, because tags inside of tags are invalid HTML
        // syntax. This occurs in cases like
        // <cr-lazy-render .template=${() => html`...
        if (templateResult.match(/<[^>]*$/)) {
          const id = placeholderId++;
          const placeholder = `${EXPR_PREFIX}-${id}`;
          map.set(placeholder, {code: nestedResult, nested: true});
          templateResult += placeholder;
        } else {
          templateResult += nestedResult;
        }
      }

      // span.literal.text contains the static HTML text following the
      // expression.
      templateResult += span.literal.text;
    });

    // Return pure content without trailing backtick
    return templateResult;
  }

  const program = ts.createProgram([filePath], {});
  const sourceFile = program.getSourceFile(filePath);
  assert.ok(sourceFile, `Could not read source file: ${filePath}`);

  code = sourceFile.text;
  const getHtmlFn = sourceFile.statements.find(
      node => ts.isFunctionDeclaration(node) && node.name &&
          node.name.text === 'getHtml');

  // Graceful early return for cases missing getHtml().
  if (!getHtmlFn) {
    return {result, map, placeholder: '', restOfFile: sourceFile.text};
  }

  const returnStatement = getHtmlFn.body.statements.find(ts.isReturnStatement);
  assert.ok(
      returnStatement && returnStatement.expression &&
      ts.isTaggedTemplateExpression(returnStatement.expression));
  const templateNode = returnStatement.expression.template;

  result = processNode(templateNode);
  const placeholder = '<!--_html_template_placeholder_-->';
  const start =
      templateNode.getStart(sourceFile) + 1;  // After opening backtick
  const end = templateNode.getEnd() - 1;      // Before closing backtick
  const restOfFile =
      code.substring(0, start) + placeholder + code.substring(end);

  return {result, map, placeholder, restOfFile};
}
