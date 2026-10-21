// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isCrLitElementType, isLiteral, isType} from './query_utils.js';

type Options = [];
type MessageIds = 'useDollarSign';

export const litUseDollarSign = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks that someElement.$.someId is used instead of someElement.shadowRoot.querySelector(\'#someId\') when someId is declared in someElement\'s $ interface.',
    },
    messages: {
      useDollarSign: 'Use {{elementExpr}}.$.{{id}} instead of {{queryExpr}}.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    return {
      'CallExpression[callee.object.property.name="shadowRoot"][callee.property.name="querySelector"]'(
          node: TSESTree.CallExpression) {
        assert.ok(isType(node, Node.CallExpression));
        assert.ok(isType(node.callee, Node.MemberExpression));
        assert.ok(isType(node.callee.object, Node.MemberExpression));

        assert.ok(node.arguments.length > 0);

        const arg0 = node.arguments[0]!;
        if (!isLiteral(arg0) || typeof arg0.value !== 'string' ||
            !arg0.value.startsWith('#')) {
          return;
        }

        const domId = arg0.value.slice(1);
        const elementNode = node.callee.object.object;

        const services = ESLintUtils.getParserServices(context);
        const checker = services.program.getTypeChecker();
        const tsNode = services.esTreeNodeToTSNodeMap.get(elementNode);
        assert.ok(tsNode);

        const type = checker.getTypeAtLocation(tsNode);
        const nonNullableType = checker.getNonNullableType(type);

        if (!isCrLitElementType(nonNullableType, checker)) {
          return;
        }

        // Ignore CrLitElement subclasses that do not declare a `$` interface.
        const dollarProp = checker.getPropertyOfType(nonNullableType, '$');
        if (!dollarProp) {
          return;
        }

        // Check whether the queried ID is declared in the `$` interface.
        const dollarType =
            checker.getTypeOfSymbolAtLocation(dollarProp, tsNode);
        const prop = checker.getPropertyOfType(dollarType, domId);
        if (!prop) {
          return;
        }

        context.report({
          node,
          messageId: 'useDollarSign',
          data: {
            elementExpr: context.sourceCode.getText(elementNode),
            id: domId,
            queryExpr: context.sourceCode.getText(node),
          },
        });
      },
    };
  },
});
