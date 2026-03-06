// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

import {dashCaseToCamelCase, LIT_IMPORT_REGEX} from './query_utils.js';

export const litElementTemplateStructure = ESLintUtils.RuleCreator.withoutDocs({
  name: 'lit-element-template-structure',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that HTML templates are not used for a Lit element\'s business logic, which should be contained in the class definition instead',
      recommended: 'error',
    },
    messages: {
      ifStatementFound:
          'If statement found in getHtml() method. Use ternary statements for conditional rendering, and delegate more complex logic to the class definition file',
      forStatementFound:
          'For loop found in getHtml() method. Use Array#map() to render the same HTML for an array of items, and delegate more complex logic to the class definition file',
      variableDeclarationFound:
          'Local (const/let) variable \'{{variableName}}\' found in the HTML template file. Logic should be delegated to the class definition file',
      functionDefinitionFound:
          'Extra function definition \'{{functionName}}\' found in the HTML template file. Complex logic should be delegated to the class definition file. Standalone/separate chunks of templates may need a dedicated custom element',
      incorrectEventListenerNameFound:
          'Incorrect event listener naming found for event \'{{eventName}}\'. Rename \'{{listenerName}}\' to follow the \'{{suggestedListenerName}}\' pattern',
    },
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    let hasLitImport = false;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](node) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name!="getHtml"]'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'functionDefinitionFound',
          data: {
            functionName: node.id.name,
          },
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] TemplateLiteral'](node) {
        const listenerExpressionRegex = /@(?<eventName>[a-zA-Z-]+)="$/;
        for (let i = 0; i < node.quasis.length; i++) {
          const match = listenerExpressionRegex.exec(node.quasis[i].value.raw);
          if (!match) {
            continue;
          }

          const eventName = match.groups['eventName'];
          if (node.expressions[i].type !== 'MemberExpression') {
            // Ignore the following pattern for now.
            // @dragenter="${this.dragAndDropHandler_?.handleDragEnter}"
            return;
          }

          if (node.expressions[i].object.type !== 'ThisExpression') {
            // Ignore the following pattern for now.
            // @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
            return;
          }

          const listenerName = node.expressions[i].property.name;
          const listenerNameRegex = new RegExp(`on(?<context>[a-zA-Z0-9]+)?${
              dashCaseToCamelCase('-' + eventName)}[_]?$`);

          if (!listenerNameRegex.test(listenerName)) {
            const camelCaseEventName = dashCaseToCamelCase('-' + eventName);
            let suggestedListenerName =
                `on<OptionalContext>${camelCaseEventName}`;
            if (listenerName.endsWith('_')) {
              suggestedListenerName += '_';
            }

            context.report({
              node: node.expressions[i],
              messageId: 'incorrectEventListenerNameFound',
              data: {
                eventName,
                suggestedListenerName,
                listenerName,
              },
            });
          }
        }
      },
      ['FunctionDeclaration[id.name="getHtml"] ForStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] ForOfStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] IfStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'ifStatementFound',
        });
      },
      ['VariableDeclaration'](node) {
        if (!hasLitImport) {
          return;
        }

        for (const declaration of node.declarations) {
          context.report({
            node,
            messageId: 'variableDeclarationFound',
            data: {
              variableName: declaration.id.name,
            },
          });
        }
      },
    };
  },
});
