// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {dashCaseToCamelCase, isIdentifier, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds =
    'ifStatementFound'|'forStatementFound'|'variableDeclarationFound'|
    'functionDefinitionFound'|'incorrectEventListenerNameFound';

export const litElementTemplateStructure = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that HTML templates are not used for a Lit element\'s business logic, which should be contained in the class definition instead',
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
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    let hasLitImport = false;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](
          _node: TSESTree.ImportDeclaration) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name!="getHtml"]'](
          node: TSESTree.FunctionDeclarationWithName) {
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
      ['FunctionDeclaration[id.name="getHtml"] TemplateLiteral'](
          node: TSESTree.TemplateLiteral) {
        const listenerExpressionRegex = /@(?<eventName>[a-zA-Z-]+)="$/;
        for (let i = 0; i < node.quasis.length; i++) {
          const match = listenerExpressionRegex.exec(node.quasis[i]!.value.raw);
          if (!match) {
            continue;
          }

          assert.ok(match.groups);
          const eventName = match.groups['eventName']!;
          const expression = node.expressions[i];
          assert.ok(expression);

          if (expression.type !== AST_NODE_TYPES.MemberExpression) {
            // Ignore the following pattern for now.
            // @dragenter="${this.dragAndDropHandler_?.handleDragEnter}"
            return;
          }

          if (expression.object.type !== AST_NODE_TYPES.ThisExpression) {
            // Ignore the following pattern for now.
            // @dragenter="${this.dragAndDropHandler_.handleDragEnter}"
            return;
          }

          assert.ok(isIdentifier(expression.property));
          const listenerName = expression.property.name;
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
              node: expression,
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
      ['FunctionDeclaration[id.name="getHtml"] ForStatement'](
          node: TSESTree.ForStatement) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] ForOfStatement'](
          node: TSESTree.ForOfStatement) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] IfStatement'](
          node: TSESTree.IfStatement) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'ifStatementFound',
        });
      },
      ['VariableDeclaration'](node: TSESTree.VariableDeclaration) {
        if (!hasLitImport) {
          return;
        }

        for (const declaration of node.declarations) {
          assert.ok(isIdentifier(declaration.id));
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
