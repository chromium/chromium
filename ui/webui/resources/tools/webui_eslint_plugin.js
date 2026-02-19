// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';

import {ESLintUtils} from '../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '../../../../third_party/node/node_modules/esquery/dist/esquery.esm.min.js';

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

const CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR =
    'CallExpression[callee.name=/Mixin(Lit)?$/][arguments.0.name="CrLitElement"]';

function isCrLitElementSubclass(node, programNode) {
  assert.ok(node.type === 'ClassDeclaration');

  if (!node.superClass) {
    return false;
  }

  if (node.superClass.type === 'Identifier') {
    if (node.superClass.name === 'CrLitElement') {
      // Case1: 'MyElement extends CrLitElement {...}'
      return true;
    }

    // Case2:
    // const MyElementBase = SomeMixin(CrLitElement);
    // MyElement extends MyElementBase {...}'
    const baseClassSelector = esquery.parse(
        `Program > VariableDeclaration > VariableDeclarator[id.name="${
            node.superClass.name}"] ${CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR}`);
    const matchingNodes = esquery.match(programNode, baseClassSelector);
    return matchingNodes.length > 0;
  }

  if (node.superClass.type === 'CallExpression') {
    // Case3: 'MyElement extends SomeMixin(SomeOtherMixin((CrLitElement)) {...}'
    const selector = esquery.parse(CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR);
    const matchingNodes = esquery.match(node.superClass, selector);
    return matchingNodes.length > 0;
  }

  return false;
}

function dashCaseToCamelCase(string) {
  return string.replace(/-([a-z])/g, group => group[1].toUpperCase());
}

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
      useFireHelper:
          'Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...))..',
      useFireHelperWithEventName:
          'Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...)), for event \'{{eventName}}\'.',
      incorrectClassName:
          'CrLitElement subclass {{className}} should end with the \'Element\' suffix.',
      incorrectDollarSignNotation:
          'Use camelCase instead of dash-case for DOM ids, change this.$[\'{{dashCaseName}}\'] to this.$.{{camelCaseName}}.',
      incorrectMethodDefinitionOrder:
          'Inconsistent method definition order in class {{className}}. Expected [{{expectedOrder}}], found [{{actualOrder}}].',
      missingSuperCalls:
          'Missing superclass calls for lifecycle method(s) {{lifecycleMethods}} in class {{className}}.',
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

    // The order in which boilerplate and lifecycle CrLitElement methods should
    // be defined.
    const desiredMethodDefinitionOrder = new Map([
      ['is', 0],
      ['styles', 1],
      ['render', 2],
      ['properties', 3],
      ['constructor', 4],
      ['connectedCallback', 5],
      ['disconnectedCallback', 6],
      ['willUpdate', 7],
      ['firstUpdated', 8],
      ['updated', 9],
    ]);

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

        // Set of defined lifecycle methods that require a call to the same
        // method of the super class.
        this.superCallRequired = new Set();

        // Set of calls to superclass lifecycle methods.
        this.superCallCalled = new Set();

        // Holds the order in which various methods are defined.
        // interface OrderEntry {
        //   name: string;
        //   node: MethodDefinition;
        // }
        this.methodDefinitionOrder = [];
      }

      visitClassDeclaration(node) {
        this.isLitElement =
            isCrLitElementSubclass(node, context.sourceCode.ast);
        if (!this.isLitElement) {
          return;
        }

        if (!node.id.name.endsWith('Element')) {
          context.report({
            node: node,
            messageId: 'incorrectClassName',
            data: {
              className: node.id.name,
            },
          });
        }

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

      runDollarSignNotationCheck(node) {
        if (!this.isLitElement) {
          return;
        }

        context.report({
          node,
          messageId: 'incorrectDollarSignNotation',
          data: {
            dashCaseName: node.property.value,
            camelCaseName: dashCaseToCamelCase(node.property.value),
          },
        });
      }

      runUseFireHelperCheck(node) {
        if (!this.isLitElement) {
          return;
        }

        assert.ok(node.type === 'ObjectExpression');

        const callExpressionNode = node.parent.parent;
        assert.ok(callExpressionNode.type === 'CallExpression');

        function hasProp(node, name, value) {
          return node.properties.some(prop => {
            return prop.key.name === name && prop.value.value === value;
          });
        }

        if (!hasProp(node, 'bubbles', true) ||
            !hasProp(node, 'composed', true)) {
          return;
        }

        let propertiesLength = 2;
        if (node.properties.find(prop => prop.key.name === 'detail')) {
          propertiesLength++;
        }

        if (node.properties.length > propertiesLength) {
          // Handle case where properties other than 'bubbles', 'composed',
          // 'detail' are passed.
          return;
        }

        const eventName = node.parent.arguments[0]?.value;
        context.report({
          node: callExpressionNode,
          messageId: eventName ? 'useFireHelperWithEventName' : 'useFireHelper',
          data: {
            eventName: node.parent.arguments[0]?.value,
          },
        });
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

      runMissingSuperCallsCheck() {
        if (!this.isLitElement || !this.node) {
          return;
        }

        const missing = this.superCallRequired.difference(this.superCallCalled);
        if (missing.size === 0) {
          return;
        }

        context.report({
          node: this.node,
          messageId: 'missingSuperCalls',
          data: {
            className: this.node.id.name,
            lifecycleMethods: Array.from(missing).join(', '),
          },
        });
      }

      runMethodDefinitionOrderCheck() {
        if (!this.isLitElement || !this.node) {
          return;
        }

        const actualOrder = this.methodDefinitionOrder.map(entry => entry.name);
        const expectedOrder =
            this.methodDefinitionOrder
                .sort((a, b) => {
                  return desiredMethodDefinitionOrder.get(a.name) -
                      desiredMethodDefinitionOrder.get(b.name);
                })
                .map(entry => entry.name);

        if (JSON.stringify(actualOrder) === JSON.stringify(expectedOrder)) {
          return;
        }

        context.report({
          node: this.node,
          messageId: 'incorrectMethodDefinitionOrder',
          data: {
            className: this.node.id.name,
            expectedOrder: expectedOrder.join(', '),
            actualOrder: actualOrder.join(', '),
          },
        });
      }
    }

    const METHOD_DEFINITION_SELECTOR_TEMPLATE =
        'ClassDeclaration > ClassBody > MethodDefinition[key.name=/{{methodDefinition}}/]';

    const METHOD_DEFINITION_ORDER_REGEX =
        `^(${Array.from(desiredMethodDefinitionOrder.keys()).join('|')})$`;
    const METHOD_DEFINITION_SELECTOR =
        METHOD_DEFINITION_SELECTOR_TEMPLATE.replace(
            '{{methodDefinition}}', METHOD_DEFINITION_ORDER_REGEX);

    const SUPER_CALL_REQUIRED_REGEX =
        '^(connectedCallback|disconnectedCallback|willUpdate|updated)$';
    const LIFECYCLE_METHOD_DEFINITION_SELECTOR =
        METHOD_DEFINITION_SELECTOR_TEMPLATE.replace(
            '{{methodDefinition}}', SUPER_CALL_REQUIRED_REGEX);
    const LIFECYCLE_METHOD_SUPER_CALL_SELECTOR = `${
        LIFECYCLE_METHOD_DEFINITION_SELECTOR} > FunctionExpression > BlockStatement > ExpressionStatement > CallExpression > MemberExpression[object.type="Super"][property.name=/${
        SUPER_CALL_REQUIRED_REGEX}/]`;

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
      [METHOD_DEFINITION_SELECTOR](node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.methodDefinitionOrder.push(
            {name: node.key.name, node});
      },
      [LIFECYCLE_METHOD_DEFINITION_SELECTOR](node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.superCallRequired.add(node.key.name);
      },
      [LIFECYCLE_METHOD_SUPER_CALL_SELECTOR](node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.superCallCalled.add(node.property.name);
      },
      ['MemberExpression[object.object.type="ThisExpression"][object.property.name="$"][property.type="Literal"]'](
          node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.runDollarSignNotationCheck(node);
      },
      ['CallExpression[callee.object.type="ThisExpression"][callee.property.name="dispatchEvent"] > NewExpression[callee.name="CustomEvent"] > ObjectExpression'](
          node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.runUseFireHelperCheck(node);
      },
      'ClassDeclaration:exit'(node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.runMissingStaticIsGetterCheck();
        currentClassInfo.runMissingSuperCallsCheck();
        currentClassInfo.runMethodDefinitionOrderCheck();
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
    let classDefinitionFile = sourceFiles.find(
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

    const importNodes = [];

    return {
      ['ImportDeclaration'](node) {
        if (node.specifiers.length > 0) {
          importNodes.push(node);
        }
      },
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

        if (node.id.name === 'getHtml' && !classDefinitionFile) {
          // Handle cases for sub-templates by finding the appropriate file.
          const paramSelector = esquery.parse('Identifier[name="this"]');
          const matchingNodes = esquery.match(node, paramSelector);
          const className =
              matchingNodes[0].typeAnnotation.typeAnnotation.typeName.name;
          // Find the URL of the import that imports the class.
          const classImport =
              importNodes
                  .find(importNode => {
                    return importNode.specifiers.some(specifier => {
                      return specifier.local.name === className;
                    });
                  })
                  .source.value;
          const classFile = path.basename(classImport).replace('.js', '.ts');
          classDefinitionFile =
              sourceFiles.find(f => path.basename(f.fileName) === classFile);
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
        /@(?<eventName>[a-zA-Z0-9-]+)\s*=\s*"\$\{\s*\(?.*?\)?\s*=>[\s\S]*?\}"/g;

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
        const tagNames = matches.map(match => {
          const tagNameStart =
              bodyString.substring(0, match.index).lastIndexOf('<') + 1;
          const tagNameLength = bodyString.substring(tagNameStart).indexOf(' ');
          return bodyString.substring(
              tagNameStart, tagNameStart + tagNameLength);
        });

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

const litElementTemplateStructure = ESLintUtils.RuleCreator.withoutDocs({
  name: 'lit-element-template-structure',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that HTML templates are not used for a Lit element\'s business logic, which should be contained in the class definition instead',
      recommended: 'error',
    },
    messages: {
      ifStatementFound:
          'If statement found in getHtml() method. Use ternary statements for conditional rendering, and delegate more complex logic to the class definition file',
      forStatementFound:
          'For loop found in getHtml() method. Use Array#map() to render the same HTML for an array of items, and delegate more complex logic to the class definition file',
      variableDeclarationFound:
          'Local (const/let) variable \'{{variableName}}\' found in the HTML template file. Logic should be delegated to the class definition file',
      functionDefinitionFound:
          'Extra function definition \'{{functionName}}\' found in the HTML template file. Complex logic should be delegated to the class definition file. Standalone/separate chunks of templates may need a dedicated custom element',
    },
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();
    let hasLitImport = false;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](node) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name!="getHtml"]'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'functionDefinitionFound',
          data: {
            functionName: node.id.name,
          },
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] ForStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] ForOfStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'forStatementFound',
        });
      },
      ['FunctionDeclaration[id.name="getHtml"] IfStatement'](node) {
        if (!hasLitImport) {
          return;
        }

        context.report({
          node,
          messageId: 'ifStatementFound',
        });
      },
      ['VariableDeclaration'](node) {
        if (!hasLitImport) {
          return;
        }

        for (const declaration of node.declarations) {
          context.report({
            node,
            messageId: 'variableDeclarationFound',
            data: {
              variableName: declaration.id.name,
            },
          });
        }
      },
    };
  },
});

const rules = {
  'inline-event-handler': inlineEventHandler,
  'lit-element-structure': litElementStructureRule,
  'lit-element-template-structure': litElementTemplateStructure,
  'lit-property-accessor': litPropertyAccessorRule,
  'polymer-property-declare': polymerPropertyDeclareRule,
  'polymer-property-class-member': polymerPropertyClassMemberRule,
  'web-component-missing-deps': webComponentMissingDeps,
};

export default {rules};
