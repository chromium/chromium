// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import {ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import * as ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';

import {isLiteral} from './query_utils.js';

function isLiteralBoolean(node: TSESTree.Literal): boolean {
  return node.raw === 'true' || node.raw === 'false';
}

function isTypeBoolean(type: ts.Type): boolean {
  return (type.flags & (ts.TypeFlags.Boolean | ts.TypeFlags.BooleanLiteral)) !==
      0;
}

type Options = [];
type MessageIds =
    'useAssertTrue1'|'useAssertTrue2'|'useAssertFalse1'|'useAssertFalse2';

export const noAssertEqualsBoolean = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Flag assertEquals(true/false, foo) when `foo` is statically typed as a boolean.',
    },
    messages: {
      useAssertTrue1:
          'Use assertTrue({{expr}}) instead of assertEquals(true, {{expr}}).',
      useAssertTrue2:
          'Use assertTrue({{expr}}) instead of assertEquals({{expr}}, true).',
      useAssertFalse1:
          'Use assertFalse({{expr}}) instead of assertEquals(false, {{expr}}).',
      useAssertFalse2:
          'Use assertFalse({{expr}}) instead of assertEquals({{expr}}, false).',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    return {
      'CallExpression[callee.name=\'assertEquals\']'(
          node: TSESTree.CallExpression) {
        assert.ok(node.arguments.length >= 2);
        const arg1 = node.arguments[0]!;
        const arg2 = node.arguments[1]!;

        let literalNode: TSESTree.Literal|null = null;
        let expressionNode: TSESTree.Node|null = null;

        if (isLiteral(arg1) && isLiteralBoolean(arg1)) {
          literalNode = arg1;
          expressionNode = arg2;
        } else if (isLiteral(arg2) && isLiteralBoolean(arg2)) {
          literalNode = arg2;
          expressionNode = arg1;
        }

        if (literalNode === null) {
          // Neither argument is `true/false`. Do nothing.
          return;
        }

        assert.ok(expressionNode);
        const services = ESLintUtils.getParserServices(context);
        const checker = services.program.getTypeChecker();
        const tsNode = services.esTreeNodeToTSNodeMap.get(expressionNode);
        const type = checker.getTypeAtLocation(tsNode);

        if (!isTypeBoolean(type)) {
          return;
        }

        const literalVariation = literalNode.raw === 'true' ? 'True' : 'False';
        const orderVariation = arg1 === literalNode ? 1 : 2;
        context.report({
          node,
          messageId: `useAssert${literalVariation}${orderVariation}`,
          data: {
            expr: context.sourceCode.getText(expressionNode),
          },
        });
      },
    };
  },
});
