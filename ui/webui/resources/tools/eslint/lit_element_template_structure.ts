// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import assert from 'node:assert';

import {dashCaseToCamelCase, isIdentifier, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds = 'ifStatementFound'|'switchStatementFound'|'forStatementFound'|
    'variableDeclarationFound'|'functionDefinitionFound'|
    'incorrectEventListenerNameFound'|'invalidGetHtmlReturn'|
    'invalidTernaryConsequent'|'invalidTernaryAlternate'|
    'invalidArrowFunctionReturn'|'unnecessaryNestedHtml'|
    'invalidLoopVariableName'|'invalidLoopIndexName'|'shadowedLoopVariableName';

const ALLOWED_LOOP_ITEM_NAMES = new Set(['item', '_item', '_']);
const ALLOWED_LOOP_INDEX_NAMES = new Set(['index', '_index', '_']);

enum LoopType {
  MAP = 'map',
  REPEAT = 'repeat',
  ASYNC_MAP = 'asyncMap',
}

const litTemplateQuery =
    esquery.parse('TaggedTemplateExpression[tag.name="html"]');
const loopCallQuery = esquery.parse(
    'CallExpression[callee.property.name="map"], ' +
    'CallExpression[callee.name=/^(repeat|asyncMap)$/]');
const LOOP_CALLBACK_SELECTOR = 'FunctionDeclaration[id.name="getHtml"] ' +
    ':matches(ArrowFunctionExpression, FunctionExpression)';

function isLitTemplate(node: TSESTree.Node): boolean {
  return node.type === Node.TaggedTemplateExpression &&
      node.tag.type === Node.Identifier && node.tag.name === 'html';
}

function hasLitTemplate(node: TSESTree.Node): boolean {
  return isLitTemplate(node) ||
      esquery.match(node, litTemplateQuery).length > 0;
}

type LoopCallback =
    TSESTree.ArrowFunctionExpression|TSESTree.FunctionExpression;

function getLoopCallbacks(
    node: TSESTree.CallExpression, loopType: LoopType): LoopCallback[] {
  let candidates: TSESTree.CallExpressionArgument[];
  switch (loopType) {
    case LoopType.REPEAT:
      candidates = node.arguments.slice(1, 3);
      break;
    case LoopType.ASYNC_MAP:
      candidates = node.arguments.slice(1, 2);
      break;
    case LoopType.MAP:
      candidates = node.arguments.slice(0, 1);
      break;
    default:
      assert.fail(`Unexpected loop type: ${loopType}`);
  }
  return candidates.filter((arg): arg is LoopCallback => {
    return arg.type === Node.ArrowFunctionExpression ||
        arg.type === Node.FunctionExpression;
  });
}

function getLoopType(node: TSESTree.Node): LoopType|null {
  if (node.type !== Node.CallExpression) {
    return null;
  }
  if (node.callee.type === Node.MemberExpression &&
      isIdentifier(node.callee.property) &&
      node.callee.property.name === LoopType.MAP &&
      node.arguments.length >= 1) {
    return getLoopCallbacks(node, LoopType.MAP)
               .some(cb => hasLitTemplate(cb.body)) ?
        LoopType.MAP :
        null;
  }
  if (!isIdentifier(node.callee)) {
    return null;
  }
  switch (node.callee.name) {
    case LoopType.REPEAT:
      return node.arguments.length >= 2 && node.arguments.length <= 3 ?
          LoopType.REPEAT :
          null;
    case LoopType.ASYNC_MAP:
      return node.arguments.length === 3 ? LoopType.ASYNC_MAP : null;
    default:
      return null;
  }
}

function hasNestedLoop(node: TSESTree.Node): boolean {
  return esquery.match(node, loopCallQuery).some(n => getLoopType(n) !== null);
}

function isValidAlternate(node: TSESTree.Node): boolean {
  if (isLitTemplate(node)) {
    return true;
  }
  // Case of returning "" or ''
  if (node.type === Node.Literal && node.value === '') {
    return true;
  }
  // Case of returning ``
  if (node.type === Node.TemplateLiteral && node.quasis.length === 1 &&
      node.quasis[0]!.value.raw === '') {
    return true;
  }
  // Case of returning Lit's nothing identifier.
  if (node.type === Node.Identifier && node.name === 'nothing') {
    return true;
  }
  return false;
}

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
      switchStatementFound:
          'Switch statement found in getHtml() method. Use ternary ' +
          'statements for conditional rendering, and delegate more complex ' +
          'logic to the class definition file',
      forStatementFound:
          'For loop found in getHtml() method. Use Array#map() to render the same HTML for an array of items, and delegate more complex logic to the class definition file',
      variableDeclarationFound:
          'Local (const/let) variable \'{{variableName}}\' found in the HTML template file. Logic should be delegated to the class definition file',
      functionDefinitionFound:
          'Extra function definition \'{{functionName}}\' found in the HTML template file. Complex logic should be delegated to the class definition file. Standalone/separate chunks of templates may need a dedicated custom element',
      incorrectEventListenerNameFound:
          'Incorrect event listener naming found for event \'{{eventName}}\'. Rename \'{{listenerName}}\' to follow the \'{{suggestedListenerName}}\' pattern',
      invalidGetHtmlReturn:
          'getHtml() function does not return a tagged template literal. Use the following format: \'return html`<all template and template logic goes here>`;\'',
      invalidTernaryConsequent:
          'Ternary operator in template must return a tagged template literal (html`...`) in its true branch. Invert the condition if necessary',
      invalidTernaryAlternate:
          'Ternary operator in template must return a tagged template literal (html`...`). Nested ternaries should be within the literal, i.e. \'${cond1 ? html`<template>` : html`${cond2 ? html`<template>` : \'\'}`}\'',
      invalidArrowFunctionReturn:
          'Arrow function in template returning Lit HTML must return a ' +
          'single tagged template literal directly, i.e. ' +
          '\'(item) => html`<template>`\'',
      unnecessaryNestedHtml:
          'Unnecessary nested html`...` tagged template literal found ' +
          'inside template expression. Inline the HTML directly into the ' +
          'parent template',
      invalidLoopVariableName:
          'Loop variable \'{{variableName}}\' in non-nested template loop ' +
          'should be named \'item\' (or \'_item\' / \'_\' if unused)',
      invalidLoopIndexName:
          'Loop index variable \'{{variableName}}\' in non-nested template ' +
          'loop should be named \'index\' (or \'_index\' / \'_\' if unused)',
      shadowedLoopVariableName:
          'Nested loop variable \'{{variableName}}\' shadows an enclosing ' +
          'loop variable. Use a distinct name to avoid confusion',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    let hasLitImport = false;
    const loopVarStack: string[][] = [];
    const callbackParamNames = new Map<LoopCallback, string[]>();

    function checkLoopParam(
        param: TSESTree.Parameter|undefined, allowedNames: Set<string>,
        invalidMessageId: 'invalidLoopVariableName'|'invalidLoopIndexName',
        isNested: boolean, enclosingNames: string[]) {
      if (!param || !isIdentifier(param)) {
        return;
      }
      if (!isNested) {
        if (!allowedNames.has(param.name)) {
          context.report({
            node: param,
            messageId: invalidMessageId,
            data: {variableName: param.name},
          });
        }
      } else if (
          !param.name.startsWith('_') && enclosingNames.includes(param.name)) {
        context.report({
          node: param,
          messageId: 'shadowedLoopVariableName',
          data: {variableName: param.name},
        });
      }
    }

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
        if (!hasLitImport) {
          return;
        }

        for (const expression of node.expressions) {
          if (isLitTemplate(expression)) {
            context.report({
              node: expression,
              messageId: 'unnecessaryNestedHtml',
            });
          }
        }

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

          if (expression.type !== Node.MemberExpression) {
            // Ignore the following pattern for now.
            // @dragenter="${this.dragAndDropHandler_?.handleDragEnter}"
            return;
          }

          if (expression.object.type !== Node.ThisExpression) {
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
      ['FunctionDeclaration[id.name="getHtml"]'](
          node: TSESTree.FunctionDeclaration) {
        if (!hasLitImport) {
          return;
        }

        const returnStatement = node.body.body.find(
            (statement): statement is TSESTree.ReturnStatement =>
                statement.type === Node.ReturnStatement);
        const argument = returnStatement ? returnStatement.argument : null;

        if (!argument || argument.type !== Node.TaggedTemplateExpression) {
          context.report({
            node: returnStatement ? returnStatement : node,
            messageId: 'invalidGetHtmlReturn',
          });
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
      ['FunctionDeclaration[id.name="getHtml"] SwitchStatement'](
          node: TSESTree.SwitchStatement) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'switchStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] ArrowFunctionExpression'](
          node: TSESTree.ArrowFunctionExpression) {
        if (!hasLitImport) {
          return;
        }

        if (!isLitTemplate(node.body) && hasLitTemplate(node.body)) {
          context.report({
            node: node.body,
            messageId: 'invalidArrowFunctionReturn',
          });
        }
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
      ['ConditionalExpression'](node: TSESTree.ConditionalExpression) {
        if (!hasLitImport) {
          return;
        }

        if (isLitTemplate(node.consequent)) {
          if (!isValidAlternate(node.alternate)) {
            context.report({
              node: node.alternate,
              messageId: 'invalidTernaryAlternate',
            });
          }
          return;
        }

        if (hasLitTemplate(node.consequent) || hasLitTemplate(node.alternate)) {
          context.report({
            node: node.consequent,
            messageId: 'invalidTernaryConsequent',
          });
          return;
        }
      },
      ['FunctionDeclaration[id.name="getHtml"] CallExpression'](
          node: TSESTree.CallExpression) {
        if (!hasLitImport) {
          return;
        }
        const loopType = getLoopType(node);
        if (!loopType) {
          return;
        }

        const callbacks = getLoopCallbacks(node, loopType);
        const enclosingNames = loopVarStack.flat();
        const isNested = loopVarStack.length > 0 ||
            callbacks.some(cb => hasNestedLoop(cb.body));

        for (const cb of callbacks) {
          checkLoopParam(
              cb.params[0], ALLOWED_LOOP_ITEM_NAMES, 'invalidLoopVariableName',
              isNested, enclosingNames);
          checkLoopParam(
              cb.params[1], ALLOWED_LOOP_INDEX_NAMES, 'invalidLoopIndexName',
              isNested, enclosingNames);
          const names =
              cb.params.slice(0, 2).filter(isIdentifier).map(p => p.name);
          callbackParamNames.set(cb, names);
        }
      },
      [LOOP_CALLBACK_SELECTOR](node: LoopCallback) {
        const names = callbackParamNames.get(node);
        if (names) {
          loopVarStack.push(names);
        }
      },
      [`${LOOP_CALLBACK_SELECTOR}:exit`](node: LoopCallback) {
        if (callbackParamNames.has(node)) {
          loopVarStack.pop();
        }
      },
    };
  },
});
