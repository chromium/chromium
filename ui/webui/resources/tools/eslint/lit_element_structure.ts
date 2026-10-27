// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, isCrLitElementSubclass, isCrLitElementType, isIdentifier, isLiteral, isMixinClass, isType, LIT_IMPORT_REGEX} from './query_utils.js';

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
    'inconsistentFilename'|'incorrectClassNameSuffix'|'incorrectDomNameSuffix'|
    'incorrectFilenameSuffix'|'incorrectMethodDefinitionOrder'|
    'invalidDollarSignProperty'|'missingCustomElementRegistration'|
    'missingCustomEventTypeParameter'|'missingStaticIsGetter'|
    'missingSuperCalls'|'missingTagNameRegistration'|'noConstructorParams'|
    'noNewCrLitElement'|'noReExport';

// Necessary info to track about each CrLitElement subclass definition
// encountered in the current file.
class ClassInfo {
  private readonly context: TSESLint.RuleContext<MessageIds, Options>;
  private readonly isTest: boolean;
  private readonly isMixin: boolean;

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
      node: TSESTree.ClassDeclarationWithName, isTest: boolean,
      isMixin: boolean) {
    this.context = context;
    this.node = node;
    this.name = this.node.id.name;
    this.isTest = isTest;
    this.isMixin = isMixin;
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

  runConstructorParamsCheck(node: TSESTree.MethodDefinition) {
    // Mixins are excluded because TypeScript requires any mixin class
    // constructor to declare a single '...args: any[]' rest parameter (TS2545).
    if (this.isMixin || node.value.params.length === 0) {
      return;
    }

    this.context.report({
      node,
      messageId: 'noConstructorParams',
      data: {
        className: this.name,
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

  runMissingTagNameRegistrationCheck() {
    if (this.isTest || this.isMixin || !this.domName ||
        this.hasTagNameRegistration) {
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
    if (this.node.abstract || this.isMixin ||
        this.hasCustomElementRegistration) {
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
    if (this.node.abstract || this.isMixin || this.domName) {
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
    if (this.isTest || this.isMixin) {
      // Don't proceed with filename consistency checks if running in a test
      // context or on a mixin class.
      return;
    }

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
    if (this.isMixin) {
      return;
    }

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

  runInvalidDollarSignPropertyCheck(node: TSESTree.TSPropertySignature) {
    assert.ok(node.typeAnnotation);
    assert.ok(isType(node.typeAnnotation.typeAnnotation, Node.TSTypeLiteral));

    const typeLiteral = node.typeAnnotation.typeAnnotation;
    for (const member of typeLiteral.members) {
      if (!isType(member, Node.TSPropertySignature)) {
        continue;
      }

      assert.ok(member.typeAnnotation);
      const isInvalid = member.optional ||
          isNullableOrUndefinable(member.typeAnnotation.typeAnnotation);
      if (isInvalid) {
        assert.ok(isIdentifier(member.key));
        this.context.report({
          node: member,
          messageId: 'invalidDollarSignProperty',
          data: {
            className: this.name,
            propertyName: member.key.name,
          },
        });
      }
    }
  }
}

function isNullableOrUndefinable(typeNode: TSESTree.TypeNode): boolean {
  if (isType(typeNode, Node.TSNullKeyword) ||
      isType(typeNode, Node.TSUndefinedKeyword)) {
    return true;
  }

  if (isType(typeNode, Node.TSUnionType)) {
    return typeNode.types.some(isNullableOrUndefinable);
  }

  return false;
}

function isTestFile(filename: string): boolean {
  const normalizedFilename = filename.replaceAll('\\', '/');
  return normalizedFilename.includes('chrome/test/data/webui') ||
      normalizedFilename.includes('chrome/test/data/pdf') ||
      normalizedFilename.endsWith('_test.ts');
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
  return isTestFile(filename);
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
      invalidDollarSignProperty:
          'Optional or nullable property \'{{propertyName}}\' is not allowed on the \'$\' interface of \'{{className}}\'. Elements accessed via \'$\' are expected to always exist in the DOM.',
      noReExport:
          'Re-exporting \'{{exportName}}\' from a file containing a CrLitElement subclass/mixin definition is not allowed. Import directly from the original source file instead.',
      noConstructorParams:
          'Constructor in CrLitElement subclass {{className}} must not accept parameters.',
      noNewCrLitElement:
          'Do not instantiate CrLitElement subclass \'{{className}}\' using \'new\'. Use document.createElement() or an HTML template instead.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    let importsRender = false;
    let htmlImportNode: TSESTree.Node|null = null;
    const importedNames = new Set<string>();
    const exportSpecifiers: TSESTree.ExportSpecifier[] = [];

    // Dollar sign property signatures in this file, keyed by interface name.
    const dollarProperties = new Map<string, TSESTree.TSPropertySignature>();

    const isTest = isTestFile(context.filename);

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
        LIFECYCLE_METHOD_DEFINITION_SELECTOR} > FunctionExpression > BlockStatement CallExpression > MemberExpression[object.type="Super"][property.name=/${
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
      },
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        if (!isCrLitElementSubclass(node, context)) {
          currentClassInfo = null;
          return;
        }

        assert.ok(node.id);
        currentClassInfo = new ClassInfo(
            context, node as TSESTree.ClassDeclarationWithName, isTest,
            isMixinClass(node, context));
        classInfos.set(node.id.name, currentClassInfo);
      },
      'ClassDeclaration > ClassBody > MethodDefinition[key.name="is"] > FunctionExpression > BlockStatement > ReturnStatement'(
          node: TSESTree.ReturnStatement) {
        if (!currentClassInfo) {
          return;
        }

        currentClassInfo.visitStaticGetIs(node);
      },
      [METHOD_DEFINITION_SELECTOR](node: TSESTree.MethodDefinition) {
        if (!currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.key));
        currentClassInfo.methodDefinitionOrder.push(
            {name: node.key.name, node});
      },
      'ClassDeclaration > ClassBody > MethodDefinition[kind="constructor"]'(
          node: TSESTree.MethodDefinition) {
        if (!currentClassInfo) {
          return;
        }

        currentClassInfo.runConstructorParamsCheck(node);
      },
      [LIFECYCLE_METHOD_DEFINITION_SELECTOR](node: TSESTree.MethodDefinition) {
        if (!currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.key));
        currentClassInfo.superCallRequired.add(node.key.name);
      },
      [LIFECYCLE_METHOD_SUPER_CALL_SELECTOR](node: TSESTree.MemberExpression) {
        if (!currentClassInfo) {
          return;
        }

        assert.ok(isIdentifier(node.property));
        currentClassInfo.superCallCalled.add(node.property.name);
      },
      ['MethodDefinition > FunctionExpression TSTypeReference[typeName.name="CustomEvent"]:not([typeArguments])'](
          node: TSESTree.TSTypeReference) {
        if (!currentClassInfo) {
          return;
        }

        currentClassInfo.runCustomEventTypeParameterCheck(node);
      },
      'ClassDeclaration:exit'(_node: TSESTree.ClassDeclaration) {
        if (!currentClassInfo) {
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
        if (classInfos.size === 0) {
          return;
        }
        assert.ok(node.typeAnnotation);
        const typeAnnotation = node.typeAnnotation.typeAnnotation;
        assert.ok(isType(typeAnnotation, Node.TSTypeReference));
        if (!isIdentifier(typeAnnotation.typeName)) {
          return;
        }
        const className = typeAnnotation.typeName.name;

        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitHtmlElementTagNameMapProperty(node);
        }
      },
      'ExpressionStatement > CallExpression[callee.object.name="customElements"][callee.property.name="define"]'(
          node: TSESTree.CallExpression) {
        if (classInfos.size === 0) {
          return;
        }
        assert.ok(isIdentifier(node.arguments[1]!));
        const className = node.arguments[1].name;
        const classInfo = classInfos.get(className) || null;
        if (classInfo) {
          classInfo.visitCustomElementsDefineCall(node);
        }
      },
      ['TSInterfaceDeclaration > TSInterfaceBody > TSPropertySignature[key.name="$"]'](
          node: TSESTree.TSPropertySignature) {
        assert.ok(isType(node.parent.parent, Node.TSInterfaceDeclaration));
        dollarProperties.set(node.parent.parent.id.name, node);
      },
      'ImportDeclaration > ImportSpecifier'(node: TSESTree.ImportSpecifier) {
        importedNames.add(node.local.name);
      },
      'ExportNamedDeclaration > ExportSpecifier'(
          node: TSESTree.ExportSpecifier) {
        exportSpecifiers.push(node);
      },
      'NewExpression'(node: TSESTree.NewExpression) {
        const services = ESLintUtils.getParserServices(context);
        const checker = services.program.getTypeChecker();
        const tsNode = services.esTreeNodeToTSNodeMap.get(node);
        assert.ok(tsNode);
        const type = checker.getTypeAtLocation(tsNode);
        if (isCrLitElementType(type, checker)) {
          assert.ok(isIdentifier(node.callee));
          context.report({
            node,
            messageId: 'noNewCrLitElement',
            data: {
              className: node.callee.name,
            },
          });
        }
      },
      'Program:exit'(_node: TSESTree.Program) {
        if (classInfos.size === 0) {
          return;
        }

        for (const classInfo of classInfos.values()) {
          classInfo.runMissingTagNameRegistrationCheck();
          classInfo.runMissingCustomElementRegistrationCheck();
        }

        for (const [interfaceName, dollarProp] of dollarProperties) {
          const classInfo = classInfos.get(interfaceName);
          if (classInfo) {
            classInfo.runInvalidDollarSignPropertyCheck(dollarProp);
          }
        }

        if (htmlImportNode && !canImportHtml(context.filename, importsRender)) {
          context.report({
            node: htmlImportNode,
            messageId: 'htmlImportInTsFile',
          });
        }

        for (const node of exportSpecifiers) {
          assert.ok(isType(node.parent, Node.ExportNamedDeclaration));
          assert.ok(isIdentifier(node.local));
          assert.ok(isIdentifier(node.exported));
          if (node.parent.source !== null ||
              importedNames.has(node.local.name)) {
            context.report({
              node,
              messageId: 'noReExport',
              data: {
                exportName: node.exported.name,
              },
            });
          }
        }
      },
    };
  },
});
