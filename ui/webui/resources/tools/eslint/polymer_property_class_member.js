// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

import {POLYMER_IMPORT_REGEX} from './query_utils.js';

export const polymerPropertyClassMemberRule = ESLintUtils.RuleCreator.withoutDocs({
  name: 'polymer-property-class-member',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that Polymer properties are also declared as class members',
      recommended: 'error',
    },
    messages: {
      missingClassMember:
          'Polymer property \'{{propName}}\' in class \'{{className}}\' must also be declared as a class member.',
    },
  },
  defaultOptions: [],
  create(context) {
    let isPolymerElement = false;
    let polymerProperties = null;  // Map<string, TSESTree.Node>|null
    let currentClass = null;       // TSESTree.ClassDeclaration|null

    return {
      [`ImportDeclaration[source.value=/${POLYMER_IMPORT_REGEX}/]`](node) {
        isPolymerElement = true;
      },
      'ClassDeclaration'(node) {
        polymerProperties = new Map();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
          node) {
        if (!isPolymerElement) {
          return;
        }

        polymerProperties.set(node.key.name, node);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(node) {
        if (!isPolymerElement) {
          return;
        }

        if (polymerProperties.has(node.key.name)) {
          polymerProperties.delete(node.key.name);
        }
      },
      'ClassDeclaration:exit'(node) {
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
