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

const litElementStructureRule = ESLintUtils.RuleCreator.withoutDocs({
  name: 'lit-element-structure',
  meta: {
    type: 'problem',
    fixable: 'code',
    docs: {
      description: 'Checks that the structure of a LitElement is correct',
      recommended: 'error',
    },
    messages: {
      missingStaticIsGetter:
          'Missing \'static get is() {...}\' for web component class {{className}}',
      missingTagNameRegistration:
          'Tag/class name pair registration to HTMLElementTagNameMap interface missing for {{tagName}} ↔ {{className}}.',
      missingCustomElementRegistration:
          'Missing customElements.define({{className}}.is, {{className}}) call.',
    },
  },
  defaultOptions: [],
  create(context) {
    // Whether lit.rollup.js is imported.
    let hasLitImport = false;

    // Whether operating on a test file, assuming all files end with the
    // '_test.ts' suffix.
    const isTest = context.filename.endsWith('_test.ts');

    // Regex to detect if a class is subclassing a native HTMLElement.
    const NATIVE_HTML_SUBCLASS_REGEX = /^HTML\S+Element$/g;

    // Necessary info to track about each class definition encountered in the
    // current file.
    class ClassInfo {
      constructor() {
        // Whether operating on a CrLitElement subclass.
        this.isLitElement = false;

        // Whether 'interface HTMLElementTagNameMap {...}' is specified. Only
        // applies when isTest=false.
        this.hasTagNameRegistration = false;

        // Whether customElements.define(...) is called.
        this.hasCustomElementRegistration = false;

        // The AST Node for the class definition.
        this.node = null;

        // The DOM name of the corresponding custom element.
        this.domName = '';
      }

      visitClassDeclaration(node) {
        if (!node.id.name.includes('Element') || node.superClass === null) {
          return;
        }

        if (node.superClass.type === 'Identifier' &&
            (node.superClass.name.match(NATIVE_HTML_SUBCLASS_REGEX) ||
             node.superClass.name === 'TestBrowserProxy')) {
          return;
        }

        this.isLitElement = true;
        this.node = node;
      }

      visitStaticGetIs(node) {
        if (!this.isLitElement) {
          return;
        }

        this.domName = node.argument.value;

        // Handle case where 'return 'foo-bar' as const;' is encountered.
        if (node.argument.type === 'TSAsExpression') {
          this.domName = node.argument.expression.value;
        }
      }

      visitHtmlElementTagNameMapProperty(node) {
        if (!this.isLitElement || this.hasTagNameRegistration) {
          return;
        }

        const typeName = node.typeAnnotation.typeAnnotation.typeName.name;
        this.hasTagNameRegistration =
            node.key.value === this.domName && typeName === this.node.id.name;
      }

      visitCustomElementsDefineCall(node) {
        if (!this.isLitElement) {
          return;
        }

        const arg0Correct = node.arguments[0].type === 'MemberExpression' &&
            node.arguments[0].object.name === this.node.id.name &&
            node.arguments[0].property.name === 'is';
        const arg1Correct = node.arguments[1].name === this.node.id.name;
        this.hasCustomElementRegistration = arg0Correct && arg1Correct;
      }

      runMissingTagNameRegistrationCheck() {
        if (isTest || !this.isLitElement || !this.node || !this.domName ||
            this.hasTagNameRegistration) {
          return;
        }

        context.report({
          node: this.node,
          messageId: 'missingTagNameRegistration',
          data: {
            className: this.node.id.name,
            tagName: this.domName,
          },
          fix: fixer => {
            const toAdd = `

declare global {
interface HTMLElementTagNameMap {
  '${this.domName}': ${this.node.id.name};
}
}`;
            return fixer.insertTextAfter(this.node, toAdd);
          },
        });
      }

      runMissingCustomElementRegistrationCheck() {
        if (!this.isLitElement || !this.node || this.node.abstract ||
            this.hasCustomElementRegistration) {
          return;
        }

        context.report({
          node: this.node,
          messageId: 'missingCustomElementRegistration',
          data: {
            className: this.node.id.name,
          },
        });
      }

      runMissingStaticIsGetterCheck() {
        if (!this.isLitElement || !this.node || this.node.abstract ||
            this.domName) {
          return;
        }

        context.report({
          node: this.node,
          messageId: 'missingStaticIsGetter',
          data: {
            className: this.node.id.name,
          },
        });
      }
    }

    // Info about all the class definitions encountered in this file.
    const classInfos = new Map();  // Map<string, ClassInfo>
    let currentClassInfo = null;   // ClassInfo|null

    return {
      [`ImportDeclaration[source.value=/${
          LIT_IMPORT_REGEX}/][importKind=value] > ImportSpecifier > Identifier[name="CrLitElement"]`](
          node) {
        hasLitImport = true;
      },
      'ClassDeclaration'(node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo = new ClassInfo();
        classInfos.set(node.id.name, currentClassInfo);

        currentClassInfo.visitClassDeclaration(node);
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="is"] > FunctionExpression > BlockStatement > ReturnStatement'(
          node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.visitStaticGetIs(node);
      },
      'ClassDeclaration:exit'(node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.runMissingStaticIsGetterCheck();
      },
      ['Program > TSModuleDeclaration[kind=global] > TSModuleBlock > TSInterfaceDeclaration[id.name="HTMLElementTagNameMap"] > TSInterfaceBody > TSPropertySignature'](
          node) {
        if (!hasLitImport) {
          return;
        }

        const className = node.typeAnnotation.typeAnnotation.typeName.name;
        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitHtmlElementTagNameMapProperty(node);
        }
      },
      'ExpressionStatement > CallExpression[callee.object.name="customElements"][callee.property.name="define"]'(
          node) {
        if (!hasLitImport) {
          return;
        }

        const className = node.arguments[1].name;
        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitCustomElementsDefineCall(node);
        }
      },
      'Program:exit'(node) {
        for (const [className, classInfo] of classInfos) {
          classInfo.runMissingTagNameRegistrationCheck();
          classInfo.runMissingCustomElementRegistrationCheck();
        }
      },
    };
  },
});

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

    const templateFilename = context.filename.replaceAll('\\', '/');
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

const inlineEventHandler = ESLintUtils.RuleCreator.withoutDocs({
  name: 'inline-event-handler',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that event handlers are not inlined in Lit/Polymer HTML templates',
      recommended: 'error',
    },
    messages: {
      inlineEventHandlerFound:
          'Inline event handler for event \'{{eventName}}\' found on element \'{{tagName}}\'. Do not use inline arrow functions in templates',
    },
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    // Regular expression to extract all inline lambda event handlers from a
    // string.
    const EVENT_HANDLER_REGEX =
        /<(?<tagName>[^ >\/!\n]+).*@(?<eventName>[a-zA-Z0-9-]+)\s*=\s*"\$\{\s*\(?.*?\)?\s*=>.*?\}"/g;

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
        const matches = Array.from(bodyString.matchAll(EVENT_HANDLER_REGEX));
        if (matches.length === 0) {
          return;
        }

        const eventNames = matches.map(match => match.groups['eventName']);
        const tagNames = matches.map(match => match.groups['tagName']);
        for (let i = 0; i < eventNames.length; i++) {
          context.report({
            node,
            messageId: 'inlineEventHandlerFound',
            data: {
              eventName: eventNames[i],
              tagName: tagNames[i],
            },
          });
        }
      },
    };
  },
});

const rules = {
  'lit-element-structure': litElementStructureRule,
  'lit-property-accessor': litPropertyAccessorRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
  'polymer-property-class-member': polymerPropertyClassMemberRule,
  'web-component-missing-deps': webComponentMissingDeps,
  'inline-event-handler': inlineEventHandler,
};

export default {rules};
