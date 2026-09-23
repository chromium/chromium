// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';

import {isIdentifier, isLiteral, isType} from './query_utils.js';

function isCrLitElementType(
    type: ts.Type, checker: ts.TypeChecker,
    visited: Set<ts.Type> = new Set()): boolean {
  const nonNullableType = checker.getNonNullableType(type);
  if (visited.has(nonNullableType)) {
    return false;
  }
  visited.add(nonNullableType);

  if (nonNullableType.symbol?.name === 'CrLitElement') {
    return true;
  }

  if ((nonNullableType.flags &
       (ts.TypeFlags.Any | ts.TypeFlags.Unknown | ts.TypeFlags.Never)) !== 0) {
    return false;
  }

  if ((nonNullableType.flags & ts.TypeFlags.TypeParameter) !== 0) {
    const constraint = checker.getBaseConstraintOfType(nonNullableType);
    return !!constraint && isCrLitElementType(constraint, checker, visited);
  }

  if ((nonNullableType.flags & ts.TypeFlags.Union) !== 0) {
    const union = nonNullableType as ts.UnionType;
    return union.types.length > 0 &&
        union.types.every(
            (t: ts.Type) => isCrLitElementType(t, checker, visited));
  }

  if ((nonNullableType.flags & ts.TypeFlags.Intersection) !== 0) {
    const intersection = nonNullableType as ts.IntersectionType;
    return intersection.types.some(
        (t: ts.Type) => isCrLitElementType(t, checker, visited));
  }

  if ((nonNullableType.flags & ts.TypeFlags.Object) !== 0 &&
      ((nonNullableType as ts.ObjectType).objectFlags &
       ts.ObjectFlags.ClassOrInterface) !== 0) {
    const baseTypes = checker.getBaseTypes(nonNullableType as ts.InterfaceType);
    return baseTypes.some(
        (baseType: ts.BaseType) =>
            isCrLitElementType(baseType, checker, visited));
  }

  return false;
}

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
