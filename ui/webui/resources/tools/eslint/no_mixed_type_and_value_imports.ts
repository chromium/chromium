// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

import {isType} from './query_utils.js';

type Options = [];
type MessageIds = 'noMixedTypeAndValueImports';

export const noMixedTypeAndValueImports =
    ESLintUtils.RuleCreator.withoutDocs<Options, MessageIds>({
      meta: {
        type: 'problem',
        docs: {
          description:
              'Prevents mixing type and value imports in the same statement',
        },
        messages: {
          noMixedTypeAndValueImports:
              'Do not mix type and value imports in the same statement. ' +
              'Split them into separate import statements instead',
        },
        schema: [],
      },
      defaultOptions: [],
      create(context) {
        return {
          ImportDeclaration(node: TSESTree.ImportDeclaration) {
            if (node.importKind === 'type') {
              return;
            }

            const specifiers = node.specifiers;
            const hasTypeSpecifier = specifiers.some(
                s =>
                    isType(s, Node.ImportSpecifier) && s.importKind === 'type');

            if (!hasTypeSpecifier) {
              return;
            }

            // It has at least one 'type' specifier.
            // It is mixed if it also has at least one 'value' specifier.
            const hasValueSpecifier = specifiers.some(
                s => !isType(s, Node.ImportSpecifier) ||
                    s.importKind !== 'type');

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
