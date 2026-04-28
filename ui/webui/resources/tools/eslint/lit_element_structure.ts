// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, isCrLitElementSubclass, isIdentifier, isLiteral, isType, LIT_IMPORT_REGEX} from './query_utils.js';

// The order in which boilerplate and lifecycle CrLitElement methods should
// be defined.
const desiredMethodDefinitionOrder = new Map<string, number>([
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

interface OrderEntry {
  name: string;
  node: TSESTree.MethodDefinition;
}

type Options = [];
type MessageIds = 'htmlImportInTsFile'|'inconsistentClassName'|
    'inconsistentFilename'|'incorrectClassNameSuffix'|
    'incorrectDollarSignNotation'|'incorrectDomNameSuffix'|
    'incorrectFilenameSuffix'|'incorrectMethodDefinitionOrder'|
    'missingCustomElementRegistration'|'missingCustomEventTypeParameter'|
    'missingStaticIsGetter'|'missingSuperCalls'|'missingTagNameRegistration'|
    'useFireHelper'|'useFireHelperWithEventName';

// Necessary info to track about each CrLitElement subclass definition
// encountered in the current file.
class ClassInfo {
  private readonly context: TSESLint.RuleContext<MessageIds, Options>;
  private readonly isTest: boolean;

  // Whether 'interface HTMLElementTagNameMap {...}' is specified. Only
  // applies when isTest=false.
  private hasTagNameRegistration: boolean = false;

  // Whether customElements.define(...) is called.
  private hasCustomElementRegistration: boolean = false;

  // The AST Node for the class definition.
  private node: TSESTree.ClassDeclarationWithName;

  // The name of the class.
  private name: string;

  // The DOM name of the corresponding custom element.
  private domName: string = '';

  // Set of defined lifecycle methods that require a call to the same
  // method of the super class.
  superCallRequired: Set<string> = new Set();

  // Set of calls to superclass lifecycle methods.
  superCallCalled: Set<string> = new Set();

  // Holds the order in which various methods are defined.
  methodDefinitionOrder: OrderEntry[] = [];

  constructor(
      context: TSESLint.RuleContext<MessageIds, Options>,
      node: TSESTree.ClassDeclarationWithName, isTest: boolean) {
    this.context = context;
    this.node = node;
    this.name = this.node.id.name;
    this.isTest = isTest;
  }

  visitStaticGetIs(node: TSESTree.ReturnStatement) {
    assert.ok(node.argument);

    switch (node.argument.type) {
      case Node.Literal:
        this.domName = node.argument.value as string;
        return;
      case Node.TSAsExpression:
        // Handle case where 'return 'foo-bar' as const;' is encountered.
        assert.ok(isLiteral(node.argument.expression));
        this.domName = node.argument.expression.value as string;
        return;
      default:
        assert.fail(`Unexpected type ${node.argument.type} encountered.`);
    }
  }

  visitHtmlElementTagNameMapProperty(node: TSESTree.TSPropertySignature) {
    if (this.isTest || this.hasTagNameRegistration) {
      return;
    }

    assert.ok(isLiteral(node.key));
    this.hasTagNameRegistration = node.key.value === this.domName;
  }

  visitCustomElementsDefineCall(node: TSESTree.CallExpression) {
    assert.ok(node.arguments.length === 2);
    const arg0 = node.arguments[0]!;
    const arg1 = node.arguments[1]!;

    const arg0Correct = isType(arg0, Node.MemberExpression) &&
        isIdentifier(arg0.object) && arg0.object.name === this.name &&
        isIdentifier(arg0.property) && arg0.property.name === 'is';
    const arg1Correct = isIdentifier(arg1) && arg1.name === this.name;
    this.hasCustomElementRegistration = arg0Correct && arg1Correct;
  }

  runDollarSignNotationCheck(node: TSESTree.MemberExpression) {
    assert.ok(isLiteral(node.property));
    const dashCaseName = node.property.value as string;
    this.context.report({
      node,
      messageId: 'incorrectDollarSignNotation',
      data: {
        dashCaseName,
        camelCaseName: dashCaseToCamelCase(dashCaseName),
      },
    });
  }

  runCustomEventTypeParameterCheck(node: TSESTree.TSTypeReference) {
    assert.ok(isType(node, Node.TSTypeReference));

    const parentNode = node.parent!.parent!;

    if (isIdentifier(parentNode)) {
      this.context.report({
        node: parentNode,
        messageId: 'missingCustomEventTypeParameter',
        data: {
          type: 'function parameter',
          name: parentNode.name,
        },
      });
      return;
    }

    assert.ok(isType(parentNode, Node.VariableDeclarator));
    assert.ok(isIdentifier(parentNode.id));
    this.context.report({
      node: parentNode,
      messageId: 'missingCustomEventTypeParameter',
      data: {
        type: 'variable',
        name: parentNode.id.name,
      },
    });
  }

  runUseFireHelperCheck(node: TSESTree.ObjectExpression) {
    assert.ok(isType(node, Node.ObjectExpression));

    const callExpressionNode = node.parent!.parent! as TSESTree.CallExpression;
    assert.ok(isType(callExpressionNode, Node.CallExpression));

    function hasProp(
        node: TSESTree.ObjectExpression, name: string,
        value: unknown): boolean {
      return node.properties.some(prop => {
        return isType(prop, Node.Property) && isIdentifier(prop.key) &&
            prop.key.name === name && isLiteral(prop.value) &&
            prop.value.value === value;
      });
    }

    if (!hasProp(node, 'bubbles', true) || !hasProp(node, 'composed', true)) {
      return;
    }

    let propertiesLength = 2;
    if (node.properties.find(
            prop => isType(prop, Node.Property) && isIdentifier(prop.key) &&
                prop.key.name === 'detail')) {
      propertiesLength++;
    }

    if (node.properties.length > propertiesLength) {
      // Handle case where properties other than 'bubbles', 'composed',
      // 'detail' are passed.
      return;
    }

    const callExpression = node.parent as TSESTree.NewExpression;
    let eventName: string = '';
    if (isLiteral(callExpression.arguments[0]!)) {
      eventName = callExpression.arguments[0]!.value as string;
    }

    this.context.report({
      node: callExpressionNode,
      messageId: eventName ? 'useFireHelperWithEventName' : 'useFireHelper',
      data: {
        eventName,
      },
    });
  }

  runMissingTagNameRegistrationCheck() {
    if (this.isTest || !this.domName || this.hasTagNameRegistration) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'missingTagNameRegistration',
      data: {
        className: this.name,
        tagName: this.domName,
      },
      fix: fixer => {
        const toAdd =
            `\n\ndeclare global {\n  interface HTMLElementTagNameMap {\n    '${
                this.domName}': ${this.name};\n  }\n}`;
        return fixer.insertTextAfter(this.node, toAdd);
      },
    });
  }

  runMissingCustomElementRegistrationCheck() {
    if (this.node.abstract || this.hasCustomElementRegistration) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'missingCustomElementRegistration',
      data: {
        className: this.name,
      },
    });
  }

  runMissingStaticIsGetterCheck() {
    if (this.node.abstract || this.domName) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'missingStaticIsGetter',
      data: {
        className: this.name,
      },
    });
  }

  runMissingSuperCallsCheck() {
    const missing = this.superCallRequired.difference(this.superCallCalled);
    if (missing.size === 0) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'missingSuperCalls',
      data: {
        className: this.name,
        lifecycleMethods: Array.from(missing).join(', '),
      },
    });
  }

  runConsistentFilenameDomNameCheck() {
    const basename = path.basename(this.context.filename, '.ts');

    if (basename.endsWith('_element')) {
      this.context.report({
        node: this.node,
        messageId: 'incorrectFilenameSuffix',
        data: {
          filename: basename + '.ts',
        },
      });
      return;
    }

    if (this.isTest) {
      // Don't proceed with filename consistency checks if running in a test
      // context, since it is common for custom elements to be defined in the
      // file holding the tests and not a dedicated file.
      return;
    }

    // Use the DOM name to derive the expected filename. If no DOM name exists,
    // use the class name instead.
    const candidateParts = this.domName ?
        this.domName.split('-') :
        this.name.match(/[A-Z]?[a-z]+/g)!.filter(p => p !== 'Element')
            .map(p => p.toLowerCase());

    const isBasenameConsistent = candidateParts.some((_, i) => {
      const candidateSuffix = candidateParts.slice(-i).join('_');

      // Allow a file name prefix that does not exist in the DOM name, only if
      // all the DOM name parts are reflected in the file name.
      return i === 0 ? basename.endsWith(candidateSuffix) :
                       candidateSuffix === basename;
    });

    if (isBasenameConsistent) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'inconsistentFilename',
      data: {
        filename: basename + '.ts',
        referenceType: this.domName ? 'DOM' : 'class',
        referenceName: this.domName || this.name,
      },
    });
  }

  runConsistentClassDomNameCheck() {
    if (!this.name.endsWith('Element')) {
      this.context.report({
        node: this.node,
        messageId: 'incorrectClassNameSuffix',
        data: {
          className: this.name,
        },
      });
      return;
    }

    if (this.domName === '') {
      // Handle case where the element is used as a super class for other
      // elements and has no DOM name.
      return;
    }

    if (this.domName.endsWith('-element')) {
      this.context.report({
        node: this.node,
        messageId: 'incorrectDomNameSuffix',
        data: {
          domName: this.domName,
        },
      });
      return;
    }

    const domNameParts = this.domName.split('-');
    const isClassNameConsistent = domNameParts.some((_, i) => {
      const candidateSuffix =
          dashCaseToCamelCase('-' + domNameParts.slice(i).join('-')) +
          'Element';

      // Allow a class name prefix that does not exist in the DOM name, only if
      // all the DOM name parts are reflected in the class name.
      return i === 0 ? this.name.endsWith(candidateSuffix) :
                       candidateSuffix === this.name;
    });

    if (isClassNameConsistent) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'inconsistentClassName',
      data: {
        className: this.name,
        domName: this.domName,
      },
    });
  }

  runMethodDefinitionOrderCheck() {
    const actualOrder = this.methodDefinitionOrder.map(entry => entry.name);
    const expectedOrder = this.methodDefinitionOrder
                              .sort((a, b) => {
                                return desiredMethodDefinitionOrder.get(a.name)!
                                    - desiredMethodDefinitionOrder.get(b.name)!;
                              })
                              .map(entry => entry.name);

    if (JSON.stringify(actualOrder) === JSON.stringify(expectedOrder)) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'incorrectMethodDefinitionOrder',
      data: {
        className: this.name,
        expectedOrder: expectedOrder.join(', '),
        actualOrder: actualOrder.join(', '),
      },
    });
  }
}

function canImportHtml(filename: string, importsRender: boolean): boolean {
  if (filename.endsWith('.html.ts')) {
    return true;
  }

  const normalizedFilename = filename.replaceAll('\\', '/');

  // Low level cr-elements can use html in combination with render() to
  // implement reusable complex rendering patterns (lazy rendering,
  // smart lists). Also allowing as a one-off exemption for
  // selectable-lazy-list in tab_search, which also implements a smart
  // list.
  const canUseHtmlWithRender =
      normalizedFilename.includes('ui/webui/resources/cr_elements') ||
      (normalizedFilename.includes('chrome/browser/resources/tab_search') &&
       normalizedFilename.endsWith('selectable_lazy_list.ts'));
  if (importsRender && canUseHtmlWithRender) {
    return true;
  }

  // Tests may use html to define dummy elements, e.g. for testing
  // mixins
  const isTestFile = normalizedFilename.includes('chrome/test/data/webui') ||
      normalizedFilename.includes('chrome/test/data/pdf');
  return isTestFile;
}

export const litElementStructureRule = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    fixable: 'code',
    docs: {
      description: 'Checks that the structure of a LitElement is correct',
    },
    messages: {
      htmlImportInTsFile:
          'Found import of html in file containing a CrLitElement subclass definition. Templates for CrLitElement subclasses belong in the .html.ts template file, not the class definition file.',
      useFireHelper:
          'Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...))..',
      useFireHelperWithEventName:
          'Use this.fire(...) instead of this.dispatchEvent(new CustomEvent(...)), for event \'{{eventName}}\'.',
      incorrectClassNameSuffix:
          'Class name \'{{className}}\' should end with the \'Element\' suffix.',
      incorrectDomNameSuffix:
          'DOM name \'{{domName}}\' should not end with the \'-element\' suffix.',
      incorrectFilenameSuffix:
          'File name \'{{filename}}\' should not end with the \'_element\' suffix.',
      inconsistentClassName:
          'Naming of class/dom pair {{className}} ↔ {{domName}} is inconsistent.',
      inconsistentFilename:
          'Naming of file/{{referenceType}} pair {{filename}} ↔ {{referenceName}} is inconsistent.',
      incorrectDollarSignNotation:
          'Use camelCase instead of dash-case for DOM ids, change this.$[\'{{dashCaseName}}\'] to this.$.{{camelCaseName}}.',
      incorrectMethodDefinitionOrder:
          'Inconsistent method definition order in class {{className}}. Expected [{{expectedOrder}}], found [{{actualOrder}}].',
      missingSuperCalls:
          'Missing superclass calls for lifecycle method(s) {{lifecycleMethods}} in class {{className}}.',
      missingStaticIsGetter:
          'Missing \'static get is() {...}\' for web component class {{className}}',
      missingTagNameRegistration:
          'Tag/class name pair registration to HTMLElementTagNameMap interface missing for {{tagName}} \u2194 {{className}}.',
      missingCustomElementRegistration:
          'Missing customElements.define({{className}}.is, {{className}}) call.',
      missingCustomEventTypeParameter:
          'Missing CustomEvent type parameter for {{type}} \'{{name}}\' (use CustomEvent<void> or CustomEvent<SomeType>).',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    // Whether lit.rollup.js is imported.
    let hasLitImport = false;
    let importsRender = false;
    let htmlImportNode: TSESTree.Node|null = null;

    // Whether operating on a test file, assuming all files end with the
    // '_test.ts' suffix.
    const isTest = context.filename.endsWith('_test.ts');

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

    // Info about all the CrLitElement subclass definitions encountered in this
    // file.
    const classInfos = new Map<string, ClassInfo>();
    let currentClassInfo: ClassInfo|null = null;

    return {
      [`ImportDeclaration[source.value=/${
          LIT_IMPORT_REGEX}/][importKind=value] > ImportSpecifier`](
          node: TSESTree.ImportSpecifier) {
        assert.ok(isIdentifier(node.imported));
        const importedName = node.imported.name;
        if (importedName === 'html') {
          htmlImportNode = node;
        }
        if (importedName === 'render') {
          importsRender = true;
        }
        if (importedName === 'CrLitElement') {
          hasLitImport = true;
        }
      },
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        if (!hasLitImport ||
            !isCrLitElementSubclass(node, context.sourceCode.ast)) {
          currentClassInfo = null;
          return;
        }

        assert.ok(node.id);
        currentClassInfo = new ClassInfo(
            context, node as TSESTree.ClassDeclarationWithName, isTest);
        classInfos.set(node.id.name, currentClassInfo);
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="is"] > FunctionExpression > BlockStatement > ReturnStatement'(
          node: TSESTree.ReturnStatement) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        currentClassInfo.visitStaticGetIs(node);
      },
      [METHOD_DEFINITION_SELECTOR](node: TSESTree.MethodDefinition) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.key));
        currentClassInfo.methodDefinitionOrder.push(
            {name: node.key.name, node});
      },
      [LIFECYCLE_METHOD_DEFINITION_SELECTOR](node: TSESTree.MethodDefinition) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.key));
        currentClassInfo.superCallRequired.add(node.key.name);
      },
      [LIFECYCLE_METHOD_SUPER_CALL_SELECTOR](node: TSESTree.MemberExpression) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.property));
        currentClassInfo.superCallCalled.add(node.property.name);
      },
      ['MemberExpression[object.object.type="ThisExpression"][object.property.name="$"][property.type="Literal"]'](
          node: TSESTree.MemberExpression) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        currentClassInfo.runDollarSignNotationCheck(node);
      },
      ['CallExpression[callee.object.type="ThisExpression"][callee.property.name="dispatchEvent"] > NewExpression[callee.name="CustomEvent"] > ObjectExpression'](
          node: TSESTree.ObjectExpression) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        currentClassInfo.runUseFireHelperCheck(node);
      },
      ['MethodDefinition > FunctionExpression TSTypeReference[typeName.name="CustomEvent"]:not([typeArguments])'](
          node: TSESTree.TSTypeReference) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        currentClassInfo.runCustomEventTypeParameterCheck(node);
      },
      'ClassDeclaration:exit'(_node: TSESTree.ClassDeclaration) {
        if (!hasLitImport || !currentClassInfo) {
          return;
        }

        currentClassInfo.runMissingStaticIsGetterCheck();
        currentClassInfo.runMissingSuperCallsCheck();
        currentClassInfo.runMethodDefinitionOrderCheck();
        currentClassInfo.runConsistentClassDomNameCheck();
        currentClassInfo.runConsistentFilenameDomNameCheck();
      },
      ['Program > TSModuleDeclaration[kind=global] > TSModuleBlock > TSInterfaceDeclaration[id.name="HTMLElementTagNameMap"] > TSInterfaceBody > TSPropertySignature'](
          node: TSESTree.TSPropertySignature) {
        if (!hasLitImport) {
          return;
        }

        assert.ok(node.typeAnnotation);
        const typeAnnotation = node.typeAnnotation.typeAnnotation;
        assert.ok(isType(typeAnnotation, Node.TSTypeReference));
        assert.ok(isIdentifier(typeAnnotation.typeName));
        const className = typeAnnotation.typeName.name;

        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitHtmlElementTagNameMapProperty(node);
        }
      },
      'ExpressionStatement > CallExpression[callee.object.name="customElements"][callee.property.name="define"]'(
          node: TSESTree.CallExpression) {
        if (!hasLitImport) {
          return;
        }

        assert.ok(isIdentifier(node.arguments[1]!));
        const className = node.arguments[1].name;
        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitCustomElementsDefineCall(node);
        }
      },
      'Program:exit'(_node: TSESTree.Program) {
        for (const classInfo of classInfos.values()) {
          classInfo.runMissingTagNameRegistrationCheck();
          classInfo.runMissingCustomElementRegistrationCheck();
        }

        if (htmlImportNode && classInfos.size > 0 &&
            !canImportHtml(context.filename, importsRender)) {
          context.report({
            node: htmlImportNode,
            messageId: 'htmlImportInTsFile',
          });
        }
      },
    };
  },
});
