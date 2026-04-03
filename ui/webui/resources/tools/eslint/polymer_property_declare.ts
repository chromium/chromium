// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/types/dist/index.js';
import {ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isIdentifier, isPolymerElementSubclass} from './query_utils.js';

type Options = [];
type MessageIds = 'missingDeclareKeyword'|'extraDeclareKeyword';

export const polymerPropertyDeclareRule = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks for the proper use of the \'declare\' keyword in Polymer code that uses useDefineForClassFields=true.',
    },
    messages: {
      missingDeclareKeyword:
          'Missing \'declare\' keyword when declaring Polymer property \'{{propName}}\' in class \'{{className}}\'.',
      extraDeclareKeyword:
          'Unnecessary \'declare\' keyword when declaring regular (non Polymer) property \'{{propName}}\' in class \'{{className}}\'.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    if (compilerOptions.useDefineForClassFields === false) {
      // Nothing to do if TS compiler flag 'useDefineForClassFields' is
      // explicitly set to 'false'.
      return {};
    }

    let isPolymerElement = false;
    let polymerProperties: Set<string>|null = null;
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

        polymerProperties = new Set();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
          node: TSESTree.Property) {
        if (!isPolymerElement) {
          return;
        }

        assert.ok(polymerProperties);
        assert.ok(isIdentifier(node.key));
        polymerProperties.add(node.key.name);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(
          node: TSESTree.PropertyDefinition) {
        if (!isPolymerElement) {
          return;
        }

        assert.ok(polymerProperties);
        assert.ok(currentClass);
        assert.ok(currentClass.id);
        assert.ok(isIdentifier(node.key));

        if (node.declare) {
          if (!polymerProperties.has(node.key.name)) {
            context.report({
              node,
              messageId: 'extraDeclareKeyword',
              data: {
                propName: node.key.name,
                className: currentClass.id.name,
              },
            });
          }
          return;
        }

        if (polymerProperties.has(node.key.name)) {
          context.report({
            node,
            messageId: 'missingDeclareKeyword',
            data: {
              propName: node.key.name,
              className: currentClass.id.name,
            },
          });
        }
      },
    };
  },
});
