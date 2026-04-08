// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {getLitPropertyType, isCrLitElementSubclass, isIdentifier} from './query_utils.js';

type Options = [];
type MessageIds = 'missingAccessorKeyword'|'extraAccessorKeyword'|
    'propertyTypeMismatch'|'missingClassMember';

export const litPropertyAccessorRule = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks for the proper use of the \'accessor\' keyword in Lit code that uses useDefineForClassFields=true.',
    },
    messages: {
      missingAccessorKeyword:
          'Missing \'accessor\' keyword when declaring Lit reactive property \'{{propName}}\' in class \'{{className}}\'.',
      extraAccessorKeyword:
          'Unnecessary \'accessor\' keyword when declaring regular (non Lit reactive) property \'{{propName}}\' in class \'{{className}}\'.',
      propertyTypeMismatch:
          'Property type mismatch: {{propertyName}} is declared as {{declaredType}} reactive property but is typed as {{tsType}}.',
      missingClassMember:
          'Missing class member declaration for Lit reactive property \'{{propName}}\'',
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

    let isLitElement = false;
    let litProperties: Map<string, string>|null = null;
    let seenProperties: Set<string>|null = null;
    let currentClass: TSESTree.ClassDeclaration|null = null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        isLitElement = isCrLitElementSubclass(node, context.sourceCode.ast);

        if (!isLitElement) {
          currentClass = null;
          litProperties = null;
          seenProperties = null;
          return;
        }

        litProperties = new Map();
        seenProperties = new Set();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
          node: TSESTree.Property) {
        if (!isLitElement) {
          return;
        }

        assert.ok(litProperties);
        assert.ok(node.value.type === AST_NODE_TYPES.ObjectExpression);
        assert.ok(isIdentifier(node.key));

        const typeProp = node.value.properties.find(p => {
          assert.ok(p.type === AST_NODE_TYPES.Property);
          assert.ok(isIdentifier(p.key));
          return p.key.name === 'type';
        }) as TSESTree.Property |
            null;

        // Required by Chromium's patch of Lit's reactive-element.d.ts.
        assert.ok(typeProp);
        assert.ok(isIdentifier(typeProp.value));
        litProperties.set(node.key.name, typeProp.value.name);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(
          node: TSESTree.PropertyDefinition) {
        if (!isLitElement) {
          return;
        }

        assert.ok(currentClass);
        assert.ok(litProperties);
        assert.ok(seenProperties);

        assert.ok(isIdentifier(node.key));
        assert.ok(currentClass.id);

        if (litProperties.has(node.key.name)) {
          seenProperties.add(node.key.name);
          context.report({
            node,
            messageId: 'missingAccessorKeyword',
            data: {
              propName: node.key.name,
              className: currentClass.id.name,
            },
          });
        }
      },
      'ClassDeclaration > ClassBody > AccessorProperty'(
          node: TSESTree.AccessorProperty) {
        if (!isLitElement) {
          return;
        }

        assert.ok(currentClass);
        assert.ok(litProperties);
        assert.ok(seenProperties);

        assert.ok(isIdentifier(node.key));
        assert.ok(currentClass.id);

        if (!litProperties.has(node.key.name)) {
          context.report({
            node,
            messageId: 'extraAccessorKeyword',
            data: {
              propName: node.key.name,
              className: currentClass.id.name,
            },
          });
          return;
        }

        seenProperties.add(node.key.name);

        // Check Lit property type is compatible with TS type.
        const declaredType = litProperties.get(node.key.name);
        assert.ok(declaredType);
        const checker = services.program.getTypeChecker();
        const tsNode = services.esTreeNodeToTSNodeMap.get(node);
        const tsType = checker.getTypeAtLocation(tsNode);
        const expressionLitType = getLitPropertyType(tsType, checker);

        if (declaredType !== expressionLitType) {
          const tsTypeStr = checker.typeToString(tsType);
          context.report({
            node,
            messageId: 'propertyTypeMismatch',
            data: {
              propertyName: node.key.name,
              declaredType: declaredType,
              tsType: tsTypeStr,
            },
          });
        }
      },
      'ClassDeclaration:exit'(node: TSESTree.ClassDeclaration) {
        if (!isLitElement || node !== currentClass) {
          return;
        }

        assert.ok(currentClass);
        assert.ok(litProperties);
        assert.ok(seenProperties);

        for (const [propName, _] of litProperties) {
          if (!seenProperties.has(propName)) {
            context.report({
              node: currentClass,
              messageId: 'missingClassMember',
              data: {
                propName: propName,
              },
            });
          }
        }
      },
    };
  },
});
