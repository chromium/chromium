// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/types/dist/index.js';
import {ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isIdentifier, isPolymerElementSubclass} from './query_utils.js';

type Options = [];
type MessageIds = 'missingClassMember';

export const polymerPropertyClassMemberRule = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that Polymer properties are also declared as class members',
    },
    messages: {
      missingClassMember:
          'Polymer property \'{{propName}}\' in class \'{{className}}\' must also be declared as a class member.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    let isPolymerElement = false;
    let polymerProperties: Map<string, TSESTree.Property>|null = null;
    let currentClass: TSESTree.ClassDeclaration|null = null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        isPolymerElement =
            isPolymerElementSubclass(node, context.sourceCode.ast);

        if (!isPolymerElement) {
          polymerProperties = null;
          currentClass = null;
          return;
        }

        polymerProperties = new Map();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
          node: TSESTree.Property) {
        if (!isPolymerElement) {
          return;
        }

        assert.ok(polymerProperties);
        assert.ok(isIdentifier(node.key));
        polymerProperties.set(node.key.name, node);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(
          node: TSESTree.PropertyDefinition) {
        if (!isPolymerElement) {
          return;
        }

        assert.ok(polymerProperties);
        assert.ok(isIdentifier(node.key));
        if (polymerProperties.has(node.key.name)) {
          polymerProperties.delete(node.key.name);
        }
      },
      'ClassDeclaration:exit'(node: TSESTree.ClassDeclaration) {
        if (!isPolymerElement) {
          return;
        }

        assert.ok(polymerProperties);
        assert.ok(currentClass);
        assert.ok(currentClass.id);

        for (const [key, value] of polymerProperties) {
          if (key.endsWith('Enum_') || key.endsWith('Enum')) {
            continue;
          }

          context.report({
            node,
            messageId: 'missingClassMember',
            data: {
              propName: key,
              className: currentClass.id.name,
            },
            loc: value.loc,
          });
        }
      },
    };
  },
});
