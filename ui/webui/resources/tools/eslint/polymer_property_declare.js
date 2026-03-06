// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

import {POLYMER_IMPORT_REGEX} from './query_utils.js';

export const polymerPropertyDeclareRule = ESLintUtils.RuleCreator.withoutDocs({
  name: 'polymer-property-declare',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks for the proper use of the \'declare\' keyword in Polymer code that uses useDefineForClassFields=true.',
      recommended: 'error',
    },
    messages: {
      missingDeclareKeyword:
          'Missing \'declare\' keyword when declaring Polymer property \'{{propName}}\' in class \'{{className}}\'.',
      extraDeclareKeyword:
          'Unnecessary \'declare\' keyword when declaring regular (non Polymer) property \'{{propName}}\' in class \'{{className}}\'.',
    },
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
    let polymerProperties = null;  // Set<string>|null
    let currentClass = null;       // TSESTree.ClassDeclaration|null

    return {
      [`ImportDeclaration[source.value=/${POLYMER_IMPORT_REGEX}/]`](node) {
        isPolymerElement = true;
      },
      'ClassDeclaration'(node) {
        polymerProperties = new Set();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
          node) {
        if (!isPolymerElement) {
          return;
        }

        polymerProperties.add(node.key.name);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(node) {
        if (!isPolymerElement) {
          return;
        }

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
        } else {
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
        }
      },
    };
  },
});
