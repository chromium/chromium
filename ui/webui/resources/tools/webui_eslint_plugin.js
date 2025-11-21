// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';

import {ESLintUtils} from '../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

// NOTE: Using `\u002F` instead of a forward slash, to workaround for
// https://github.com/eslint/eslint/issues/16555,
// https://eslint.org/docs/latest/extend/selectors#known-issues
// where forward slashes are not properly escaped in regular expressions
// appearing in AST selectors.
const POLYMER_IMPORT_REGEX = [
  'resources',
  'polymer',
  'v3_0',
  'polymer',
  'polymer_bundled.min.js$',
].join('\\u002F');
const LIT_IMPORT_REGEX =
    ['resources', 'lit', 'v3_0', 'lit.rollup.js$'].join('\\u002F');

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
          'Missing \'accessor\' keyword when declaring Lit reactive property \'{{propName}}\' in class \'{{className}}\'.',
      extraAccessorKeyword:
          'Unnecessary \'accessor\' keyword when declaring regular (non Lit reactive) property \'{{propName}}\' in class \'{{className}}\'.',
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
    let currentClass = null;   // TSESTree.ClassDeclaration|null

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](node) {
        isLitElement = true;
      },
      'ClassDeclaration'(node) {
        litProperties = new Set();
        currentClass = node;
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property'(
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
            data: {
              propName: node.key.name,
              className: currentClass.id.name,
            },
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

const polymerPropertyClassMemberRule = ESLintUtils.RuleCreator.withoutDocs({
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

const webComponentMissingDeps = ESLintUtils.RuleCreator.withoutDocs({
  name: 'web-component-missing-deps',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that all children of a web component are imported as dependencies',
      recommended: 'error',
    },
    messages: {
      missingImportStatement:
          'Missing explicit import statement for \'{{childName}}\' in the class definition file \'{{fileName}}\'.',
      missingImportStatementWithLazyLoad:
          'Missing explicit import statement for \'{{childName}}\' in the class definition file \'{{fileName}}\' or in \'{{lazyLoadFileName}}\'.',
    },
  },
  defaultOptions: [],
  create(context) {
    function extractImportsUrlsFromFile(file) {
      const parser = context.languageOptions.parser;
      const parserOptions = context.languageOptions.parserOptions;
      const ast = parser.parse(file.text, {
        ...parserOptions,
        filePath: file.fileName,
      });
      return ast.body
          .filter(
              node => node.type === 'ImportDeclaration' &&
                  node.specifiers.length === 0)
          .map(node => node.source.value);
    }

    const templateFilename = context.getFilename().replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    // The file where imports are expected to exist.
    const classDefinitionFile = sourceFiles.find(
        f => (f.fileName === templateFilename.replace(/\.html\.ts$/, '.ts')) ||
            null);

    // The file where any intentionally omitted dependencies from the previous
    // file are expected to exist. Only applicable to WebUIs that use lazy
    // loading (as of this writing NTP and Settings only).
    const lazyLoadFile =
        sourceFiles.find(f => path.basename(f.fileName) === 'lazy_load.ts') ||
        null;

    /*
     * Calculates a list of candidate filenames that could possibly host the
     * definition of a custom element named `tagName`. For example:
     * foo-bar-baz -> ['foo_bar_baz.js', 'bar_baz.js', 'baz.js'].
     */
    function getExpectedImportFilenames(tagName) {
      const parts = tagName.split('-');
      const filenames = new Set();
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
      ['FunctionDeclaration[id.name=/getHtml|getTemplate/]'](node) {
        // Looking for either of the following patterns
        //  - Lit templates: 'getHtml(this: SomeType) {...}'
        //  - Polymer templates: 'getTemplate() {...}'

        if (node.id.name === 'getHtml' &&
            (node.params.length !== 1 || node.params[0].name !== 'this')) {
          // Handle a few cases where lit-html is used directly and there is no
          // classDefinitionFilename file.
          return;
        }

        // Extract function's body as a string.
        const bodyString = context.getSourceCode().getText(node.body);

        // Extract all elements used in the function. Filter out tag names that
        // don't include any '-' characters since these are native HTML
        // elements.
        const matches = Array.from(bodyString.matchAll(TAG_NAME_REGEX));
        const tagNames = new Set(matches.map(match => match.groups['tagName'])
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
        let lazyImportedFileNames = null;

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

const rules = {
  'lit-property-accessor': litPropertyAccessorRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
  'polymer-property-class-member': polymerPropertyClassMemberRule,
  'web-component-missing-deps': webComponentMissingDeps,
};

export default {rules};
