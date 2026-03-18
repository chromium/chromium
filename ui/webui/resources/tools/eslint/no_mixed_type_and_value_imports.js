// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

export const noMixedTypeAndValueImports = ESLintUtils.RuleCreator.withoutDocs({
  name: 'no-mixed-type-and-value-imports',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Prevents mixing type and value imports in the same statement',
      recommended: 'error',
    },
    messages: {
      noMixedTypeAndValueImports:
          'Do not mix type and value imports in the same statement. ' +
          'Split them into separate import statements instead',
    },
  },
  defaultOptions: [],
  create(context) {
    return {
      ImportDeclaration(node) {
        if (node.importKind === 'type') {
          return;
        }

        const specifiers = node.specifiers;
        const hasTypeSpecifier = specifiers.some(
            s => s.type === 'ImportSpecifier' && s.importKind === 'type');

        if (!hasTypeSpecifier) {
          return;
        }

        // It has at least one 'type' specifier.
        // It is mixed if it also has at least one 'value' specifier.
        const hasValueSpecifier = specifiers.some(
            s => s.type !== 'ImportSpecifier' || s.importKind !== 'type');

        if (hasValueSpecifier) {
          context.report({
            node,
            messageId: 'noMixedTypeAndValueImports',
          });
        }
      },
    };
  },
});
