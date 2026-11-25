// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';

import {getLitPropertyType, isCrLitElementSubclass, isIdentifier, isLiteral, isType, PROPERTIES_METHOD_PROPERTY_SELECTOR} from './query_utils.js';

type Options = [];
type MessageIds =
    'missingAccessorKeyword'|'extraAccessorKeyword'|'propertyTypeMismatch'|
    'missingClassMember'|'unnecessaryReactiveLoadTimeDataBoolean';

interface LitPropertyInfo {
  type: string;
  hasReflectOrNotify: boolean;
}

function isLoadTimeDataCall(
    node: TSESTree.Expression, methodNames: string[]): boolean {
  return isType(node, Node.CallExpression) &&
      isType(node.callee, Node.MemberExpression) && !node.callee.computed &&
      isIdentifier(node.callee.object) &&
      node.callee.object.name === 'loadTimeData' &&
      isIdentifier(node.callee.property) &&
      methodNames.includes(node.callee.property.name);
}

function isStaticBooleanSubtree(
    node: TSESTree.Expression, state: {hasGetBoolean: boolean}): boolean {
  // Direct loadTimeData.getBoolean('foo') call.
  if (isLoadTimeDataCall(node, ['getBoolean'])) {
    state.hasGetBoolean = true;
    return true;
  }
  // Guard calls like loadTimeData.valueExists('foo') or
  // loadTimeData.isInitialized().
  if (isLoadTimeDataCall(node, ['valueExists', 'isInitialized'])) {
    return true;
  }
  // Boolean literals (true / false), e.g. fallback branches in ternaries.
  if (isLiteral(node) && typeof node.value === 'boolean') {
    return true;
  }
  // Logical negation, e.g. !loadTimeData.getBoolean('foo').
  if (isType(node, Node.UnaryExpression) && node.operator === '!') {
    return isStaticBooleanSubtree(node.argument, state);
  }
  // Logical binary expressions (&&, ||, ??), e.g.
  // loadTimeData.valueExists('foo') && loadTimeData.getBoolean('foo').
  if (isType(node, Node.LogicalExpression)) {
    return isStaticBooleanSubtree(node.left, state) &&
        isStaticBooleanSubtree(node.right, state);
  }
  // Ternary expressions, e.g.
  // loadTimeData.valueExists('foo') ? loadTimeData.getBoolean('foo') : false.
  if (isType(node, Node.ConditionalExpression)) {
    return isStaticBooleanSubtree(node.test, state) &&
        isStaticBooleanSubtree(node.consequent, state) &&
        isStaticBooleanSubtree(node.alternate, state);
  }
  return false;
}

function isStaticLoadTimeDataBoolean(expr: TSESTree.Expression): boolean {
  const state = {hasGetBoolean: false};
  return isStaticBooleanSubtree(expr, state) && state.hasGetBoolean;
}

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
      unnecessaryReactiveLoadTimeDataBoolean:
          'Property \'{{propName}}\' in class \'{{className}}\' is initialized from a static loadTimeData boolean and should not be a reactive Lit property. Remove it from \'properties()\', remove \'accessor\', and mark it as \'readonly\' instead.',
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
    let litProperties: Map<string, LitPropertyInfo>|null = null;
    let seenProperties: Set<string>|null = null;
    let loadTimeDataBooleanCandidates: Map<string, TSESTree.AccessorProperty>|
        null = null;
    let reassignedProperties: Set<string>|null = null;
    let currentClass: TSESTree.ClassDeclaration|null = null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        isLitElement = isCrLitElementSubclass(node, context);

        if (!isLitElement) {
          currentClass = null;
          litProperties = null;
          seenProperties = null;
          loadTimeDataBooleanCandidates = null;
          reassignedProperties = null;
          return;
        }

        litProperties = new Map();
        seenProperties = new Set();
        loadTimeDataBooleanCandidates = new Map();
        reassignedProperties = new Set();
        currentClass = node;
      },
      [`ClassDeclaration > ${PROPERTIES_METHOD_PROPERTY_SELECTOR}`](
          node: TSESTree.Property) {
        if (!isLitElement) {
          return;
        }

        assert.ok(litProperties);
        assert.ok(isType(node.value, Node.ObjectExpression));
        assert.ok(isIdentifier(node.key));

        const typeProp = node.value.properties.find(p => {
          assert.ok(isType(p, Node.Property));
          assert.ok(isIdentifier(p.key));
          return p.key.name === 'type';
        }) as TSESTree.Property |
            null;

        // Required by Chromium's patch of Lit's reactive-element.d.ts.
        assert.ok(typeProp);
        assert.ok(isIdentifier(typeProp.value));

        const hasReflectOrNotify = node.value.properties.some(p => {
          return isType(p, Node.Property) && isIdentifier(p.key) &&
              (p.key.name === 'reflect' || p.key.name === 'notify') &&
              isLiteral(p.value) && p.value.value === true;
        });
        litProperties.set(node.key.name, {
          type: typeProp.value.name,
          hasReflectOrNotify,
        });
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
        assert.ok(loadTimeDataBooleanCandidates);

        assert.ok(isIdentifier(node.key));
        assert.ok(currentClass.id);

        const propInfo = litProperties.get(node.key.name);
        if (!propInfo) {
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
        const declaredType = propInfo.type;
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
          return;
        }

        const flags = ts.getCombinedModifierFlags(tsNode);
        const isNonPublic =
            (flags & ts.ModifierFlags.NonPublicAccessibilityModifier) !== 0;
        const isOverride = (flags & ts.ModifierFlags.Override) !== 0;
        if (isNonPublic && !isOverride && declaredType === 'Boolean' &&
            !propInfo.hasReflectOrNotify && node.value !== null &&
            isStaticLoadTimeDataBoolean(node.value)) {
          loadTimeDataBooleanCandidates.set(node.key.name, node);
        }
      },
      'AssignmentExpression'(node: TSESTree.AssignmentExpression) {
        if (!isLitElement) {
          return;
        }

        assert.ok(reassignedProperties);
        if (isType(node.left, Node.MemberExpression) &&
            isType(node.left.object, Node.ThisExpression) &&
            !node.left.computed && isIdentifier(node.left.property)) {
          reassignedProperties.add(node.left.property.name);
        }
      },
      'ClassDeclaration:exit'(node: TSESTree.ClassDeclaration) {
        if (!isLitElement || node !== currentClass) {
          return;
        }

        assert.ok(currentClass);
        assert.ok(currentClass.id);
        assert.ok(litProperties);
        assert.ok(seenProperties);
        assert.ok(loadTimeDataBooleanCandidates);
        assert.ok(reassignedProperties);

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

        for (const [propName, propNode] of loadTimeDataBooleanCandidates) {
          if (!reassignedProperties.has(propName)) {
            context.report({
              node: propNode,
              messageId: 'unnecessaryReactiveLoadTimeDataBoolean',
              data: {
                propName,
                className: currentClass.id.name,
              },
            });
          }
        }

        isLitElement = false;
        currentClass = null;
        litProperties = null;
        seenProperties = null;
        loadTimeDataBooleanCandidates = null;
        reassignedProperties = null;
      },
    };
  },
});
