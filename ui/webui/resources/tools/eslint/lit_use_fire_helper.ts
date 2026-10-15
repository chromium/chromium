// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isCrLitElementType, isIdentifier, isLiteral, isType} from './query_utils.js';

type Options = [];
type MessageIds = 'useFireHelper'|'useFireHelperWithEventName';

export const litUseFireHelper = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks that fire() is used instead of dispatchEvent(new CustomEvent(...)) on CrLitElement instances.',
    },
    messages: {
      useFireHelper:
          'Use {{expr}}.fire(...) instead of {{expr}}.dispatchEvent(new CustomEvent(...)).',
      useFireHelperWithEventName:
          'Use {{expr}}.fire(...) instead of {{expr}}.dispatchEvent(new CustomEvent(...)), for event \'{{eventName}}\'.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    return {
      ['CallExpression[callee.property.name="dispatchEvent"] > NewExpression[callee.name="CustomEvent"] > ObjectExpression'](
          node: TSESTree.ObjectExpression) {
        assert.ok(isType(node, Node.ObjectExpression));

        const callExpressionNode =
            node.parent!.parent! as TSESTree.CallExpression;
        assert.ok(isType(callExpressionNode, Node.CallExpression));

        function hasProp(
            node: TSESTree.ObjectExpression, name: string,
            value: unknown): boolean {
          return node.properties.some(prop => {
            return isType(prop, Node.Property) && isIdentifier(prop.key) &&
                prop.key.name === name && isLiteral(prop.value) &&
                prop.value.value === value;
          });
        }

        if (!hasProp(node, 'bubbles', true) ||
            !hasProp(node, 'composed', true)) {
          return;
        }

        let propertiesLength = 2;
        if (node.properties.find(
                prop => isType(prop, Node.Property) && isIdentifier(prop.key) &&
                    prop.key.name === 'detail')) {
          propertiesLength++;
        }

        if (node.properties.length > propertiesLength) {
          // Handle case where properties other than 'bubbles', 'composed',
          // 'detail' are passed.
          return;
        }

        const callee = callExpressionNode.callee;
        assert.ok(isType(callee, Node.MemberExpression));
        const calleeObject = callee.object;

        const services = ESLintUtils.getParserServices(context);
        const checker = services.program.getTypeChecker();
        const tsNode = services.esTreeNodeToTSNodeMap.get(calleeObject);
        assert.ok(tsNode);
        const type = checker.getTypeAtLocation(tsNode);

        if (!isCrLitElementType(type, checker)) {
          return;
        }

        const newExpression = node.parent as TSESTree.NewExpression;
        let eventName: string = '';
        if (isLiteral(newExpression.arguments[0]!)) {
          eventName = newExpression.arguments[0]!.value as string;
        }

        context.report({
          node: callExpressionNode,
          messageId: eventName ? 'useFireHelperWithEventName' : 'useFireHelper',
          data: {
            expr: context.sourceCode.getText(calleeObject),
            eventName,
          },
        });
      },
    };
  },
});
