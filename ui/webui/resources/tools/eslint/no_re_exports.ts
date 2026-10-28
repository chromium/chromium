// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isIdentifier, isType} from './query_utils.js';

type Options = [];
type MessageIds = 'noReExport';

// Returns whether the file is a top-level barrel file (e.g. settings.ts,
// lazy_load.ts), which only contains import and re-export statements and no
// local definitions or executable statements, used to expose a WebUI's public
// API to tests (at most two per build_webui() target).
function isTopLevelBarrelFile(program: TSESTree.Program): boolean {
  return program.body.every(stmt => {
    return isType(stmt, Node.ImportDeclaration) ||
        isType(stmt, Node.ExportAllDeclaration) ||
        (isType(stmt, Node.ExportNamedDeclaration) &&
         stmt.declaration === null);
  });
}

export const noReExports =
    ESLintUtils.RuleCreator.withoutDocs<Options, MessageIds>({
      meta: {
        type: 'problem',
        docs: {
          description:
              'Prevents re-exporting symbols except in top-level barrel files',
        },
        messages: {
          noReExport:
              'Re-exporting \'{{exportName}}\' is not allowed, except in ' +
              'top-level barrel files (files with only import/export ' +
              'statements). Import directly from the original source file ' +
              'instead.',
        },
        schema: [],
      },
      defaultOptions: [],
      create(context) {
        const importedNames = new Set<string>();
        const exportSpecifiers: TSESTree.ExportSpecifier[] = [];

        return {
          'ImportDeclaration > ImportSpecifier'(
              node: TSESTree.ImportSpecifier) {
            importedNames.add(node.local.name);
          },
          'ExportNamedDeclaration > ExportSpecifier'(
              node: TSESTree.ExportSpecifier) {
            exportSpecifiers.push(node);
          },
          'Program:exit'(node: TSESTree.Program) {
            if (exportSpecifiers.length === 0 || isTopLevelBarrelFile(node)) {
              return;
            }

            for (const specifier of exportSpecifiers) {
              assert.ok(isType(specifier.parent, Node.ExportNamedDeclaration));
              assert.ok(isIdentifier(specifier.local));
              assert.ok(isIdentifier(specifier.exported));
              if (specifier.parent.source !== null ||
                  importedNames.has(specifier.local.name)) {
                context.report({
                  node: specifier,
                  messageId: 'noReExport',
                  data: {
                    exportName: specifier.exported.name,
                  },
                });
              }
            }
          },
        };
      },
    });
