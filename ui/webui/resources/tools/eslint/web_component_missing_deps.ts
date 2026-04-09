// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type * as ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {extractClassImport, isType} from './query_utils.js';

type Options = [];
type MessageIds = 'missingImportStatement'|'missingImportStatementWithLazyLoad';

export const webComponentMissingDeps = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that all children of a web component are imported as dependencies',
    },
    messages: {
      missingImportStatement:
          'Missing explicit import statement for \'{{childName}}\' in the class definition file \'{{fileName}}\'.',
      missingImportStatementWithLazyLoad:
          'Missing explicit import statement for \'{{childName}}\' in the class definition file \'{{fileName}}\' or in \'{{lazyLoadFileName}}\'.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    function extractImportsUrlsFromFile(file: ts.SourceFile): string[] {
      const parserOptions = context.languageOptions.parserOptions;
      const parser =
          context.languageOptions.parser as TSESLint.Parser.ParserModule;
      assert.ok('parse' in parser);
      const ast = parser.parse(file.text, {
        ...parserOptions,
        filePath: file.fileName,
      });
      return ast.body
          .filter(
              (node: TSESTree.ProgramStatement):
                  node is TSESTree.ImportDeclaration =>
                  isType(node, Node.ImportDeclaration) &&
                  node.specifiers.length === 0)
          .map(node => node.source.value);
    }

    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    // The file where imports are expected to exist.
    let classDefinitionFile =
        sourceFiles.find(
            f => f.fileName ===
                templateFilename.replace(/\.html\.ts$/, '.ts')) ||
        null;

    // The file where any intentionally omitted dependencies from the previous
    // file are expected to exist. Only applicable to WebUIs that use lazy
    // loading (as of this writing NTP and Settings only).
    const lazyLoadFile =
        sourceFiles.find(f => path.basename(f.fileName) === 'lazy_load.ts') ||
        null;

    /*
     * Calculates a list of candidate filenames that could possibly host the
     * definition of a custom element named \`tagName\`. For example:
     * foo-bar-baz -> ['foo_bar_baz.js', 'bar_baz.js', 'baz.js'].
     */
    function getExpectedImportFilenames(tagName: string): Set<string> {
      const parts = tagName.split('-');
      const filenames = new Set<string>();
      for (let i = 0; i < parts.length; i++) {
        filenames.add(parts.slice(i).join('_') + '.js');
        if (tagName === 'iron-list') {
          filenames.add(parts.slice(i).join('-') + '.js');
        }
      }

      return filenames;
    }

    // Regular expression to extract all DOM tag names from a string.
    const TAG_NAME_REGEX = /<(?<tagName>[^ >\/!\n]+)/g;

    return {
      ['FunctionDeclaration[id.name=/getHtml|getTemplate/]'](
          node: TSESTree.FunctionDeclarationWithName) {
        // Looking for either of the following patterns
        //  - Lit templates: 'getHtml(this: SomeType) {...}'
        //  - Polymer templates: 'getTemplate() {...}'

        if (node.id.name === 'getHtml') {
          const classParams = extractClassImport(node, context.sourceCode.ast);

          // Handle a few cases where lit-html is used directly and there is no
          // classDefinitionFilename file.
          if (classParams.type === '') {
            return;
          }

          // Handle cases for sub-templates by finding the appropriate file.
          if (classDefinitionFile === null) {
            classDefinitionFile =
                sourceFiles.find(
                    f => path.basename(f.fileName) === classParams.fileName) ||
                null;
          }
        }

        // Extract function's body as a string.
        const bodyString = context.sourceCode.getText(node.body);

        // Extract all elements used in the function. Filter out tag names that
        // don't include any '-' characters since these are native HTML
        // elements.
        const matches = Array.from(bodyString.matchAll(TAG_NAME_REGEX));
        const tagNames = new Set(matches.map(match => match.groups!['tagName']!)
                                     .filter(tagName => tagName.includes('-')));

        if (tagNames.size === 0) {
          // No custom element children detected. Nothing to check.
          return;
        }

        // Compare tagNames against the list of imports.

        // List of directly imported files.
        assert.ok(classDefinitionFile !== null);
        const importedFilenames =
            new Set(extractImportsUrlsFromFile(classDefinitionFile)
                        .map(url => path.basename(url)));

        // List of lazily imported files.
        let lazyImportedFileNames: Set<string>|null = null;

        for (const tagName of tagNames) {
          const expectedImportedFilenames = getExpectedImportFilenames(tagName);

          if (importedFilenames.intersection(expectedImportedFilenames).size >
              0) {
            continue;
          }

          if (expectedImportedFilenames.has(
                  path.basename(templateFilename).replace('.html.ts', '.js'))) {
            // Handle edge case of self referencing web component.
            continue;
          }

          if (lazyImportedFileNames === null && lazyLoadFile !== null) {
            lazyImportedFileNames =
                new Set(extractImportsUrlsFromFile(lazyLoadFile)
                            .map(url => path.basename(url)));
          }

          if (lazyImportedFileNames !== null &&
              lazyImportedFileNames.intersection(expectedImportedFilenames)
                      .size > 0) {
            // Handle case where the dependency is imported lazily (meaning in
            // lazy_load.ts).
            continue;
          }

          // Report errors for each tagName that is not explicitly imported.
          if (lazyLoadFile === null) {
            context.report({
              node,
              messageId: 'missingImportStatement',
              data: {
                childName: tagName,
                fileName: path.basename(classDefinitionFile.fileName),
              },
            });
          } else {
            context.report({
              node,
              messageId: 'missingImportStatementWithLazyLoad',
              data: {
                childName: tagName,
                fileName: path.basename(classDefinitionFile.fileName),
                lazyLoadFileName: path.basename(lazyLoadFile.fileName),
              },
            });
          }
        }
      },
    };
  },
});
