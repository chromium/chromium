// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

const POLYMER_IMPORT =
    '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
const LIT_IMPORT = '//resources/lit/v3_0/lit.rollup.js';

const litPropertyAccessorRule = ESLintUtils.RuleCreator.withoutDocs({
  name: 'lit-property-accessor',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Checks for the proper use of the \'accessor\' keyword in Lit code that uses useDefineForClassFields=true.',
      recommended: 'error',
    },
    messages: {
      missingAccessorKeyword:
          'Missing \'accessor\' keyword when declaring Litreactive property \'{{propName}}\'.',
      extraAccessorKeyword:
          'Unnecessary \'accessor\' keyword when declaring regular (non Lit reactive) property \'{{propName}}\'.',
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

    let isLitElement = false;
    let litProperties = null;  // Set<string>|null

    return {
      [`ImportDeclaration[source.value='${LIT_IMPORT}']`](node) {
        isLitElement = true;
      },
      'ClassDeclaration'(node) {
        litProperties = new Set();
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] ReturnStatement > ObjectExpression > Property'(
          node) {
        if (!isLitElement) {
          return;
        }

        litProperties.add(node.key.name);
      },
      'ClassDeclaration > ClassBody > PropertyDefinition'(node) {
        if (!isLitElement) {
          return;
        }

        if (litProperties.has(node.key.name)) {
          context.report({
            node,
            messageId: 'missingAccessorKeyword',
            data: {propName: node.key.name},
          });
        }
      },
      'ClassDeclaration > ClassBody > AccessorProperty'(node) {
        if (!isLitElement) {
          return;
        }

        if (!litProperties.has(node.key.name)) {
          context.report({
            node,
            messageId: 'extraAccessorKeyword',
            data: {propName: node.key.name},
          });
        }
      },
    };
  },
});

const polymerPropertyDeclareRule = ESLintUtils.RuleCreator.withoutDocs({
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
          'Missing \'declare\' keyword when declaring Polymer property \'{{propName}}\'.',
      extraDeclareKeyword:
          'Unnecessary \'declare\' keyword when declaring regular (non Polymer) property \'{{propName}}\'.',
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

    return {
      [`ImportDeclaration[source.value='${POLYMER_IMPORT}']`](node) {
        isPolymerElement = true;
      },
      'ClassDeclaration'(node) {
        polymerProperties = new Set();
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] ReturnStatement > ObjectExpression > Property'(
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
              data: {propName: node.key.name},
            });
          }
        } else {
          if (polymerProperties.has(node.key.name)) {
            context.report({
              node,
              messageId: 'missingDeclareKeyword',
              data: {propName: node.key.name},
            });
          }
        }
      },
    };
  },
});

const rules = {
  'lit-property-accessor': litPropertyAccessorRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
};

export default {rules};
