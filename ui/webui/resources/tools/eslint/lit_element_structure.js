// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';

import {dashCaseToCamelCase, isCrLitElementSubclass, LIT_IMPORT_REGEX} from './query_utils.js';

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
  constructor(context, isTest) {
    this.context = context;
    this.isTest = isTest;
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
        isCrLitElementSubclass(node, this.context.sourceCode.ast);
    if (!this.isLitElement) {
      return;
    }

    if (!node.id.name.endsWith('Element')) {
      this.context.report({
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

    this.context.report({
      node,
      messageId: 'incorrectDollarSignNotation',
      data: {
        dashCaseName: node.property.value,
        camelCaseName: dashCaseToCamelCase(node.property.value),
      },
    });
  }

  runCustomEventTypeParameterCheck(node) {
    assert.ok(node.type === 'TSTypeReference');

    if (!this.isLitElement) {
      return;
    }

    const parentNode = node.parent.parent;

    if (parentNode.type === 'Identifier') {
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

    assert.ok(parentNode.type === 'VariableDeclarator');
    this.context.report({
      node: parentNode,
      messageId: 'missingCustomEventTypeParameter',
      data: {
        type: 'variable',
        name: parentNode.id.name,
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

    if (!hasProp(node, 'bubbles', true) || !hasProp(node, 'composed', true)) {
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
    this.context.report({
      node: callExpressionNode,
      messageId: eventName ? 'useFireHelperWithEventName' : 'useFireHelper',
      data: {
        eventName: node.parent.arguments[0]?.value,
      },
    });
  }

  runMissingTagNameRegistrationCheck() {
    if (this.isTest || !this.isLitElement || !this.node || !this.domName ||
        this.hasTagNameRegistration) {
      return;
    }

    this.context.report({
      node: this.node,
      messageId: 'missingTagNameRegistration',
      data: {
        className: this.node.id.name,
        tagName: this.domName,
      },
      fix: fixer => {
        const toAdd =
            `\n\ndeclare global {\n  interface HTMLElementTagNameMap {\n    '${
                this.domName}': ${this.node.id.name};\n  }\n}`;
        return fixer.insertTextAfter(this.node, toAdd);
      },
    });
  }

  runMissingCustomElementRegistrationCheck() {
    if (!this.isLitElement || !this.node || this.node.abstract ||
        this.hasCustomElementRegistration) {
      return;
    }

    this.context.report({
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

    this.context.report({
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

    this.context.report({
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

    this.context.report({
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

export const litElementStructureRule = ESLintUtils.RuleCreator.withoutDocs({
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
          'Tag/class name pair registration to HTMLElementTagNameMap interface missing for {{tagName}} \u2194 {{className}}.',
      missingCustomElementRegistration:
          'Missing customElements.define({{className}}.is, {{className}}) call.',
      missingCustomEventTypeParameter:
          'Missing CustomEvent type parameter for {{type}} \'{{name}}\' (use CustomEvent<void> or CustomEvent<SomeType>).',
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

        currentClassInfo = new ClassInfo(context, isTest);
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
      ['MethodDefinition > FunctionExpression TSTypeReference[typeName.name="CustomEvent"]:not([typeArguments])'](
          node) {
        if (!hasLitImport) {
          return;
        }

        currentClassInfo.runCustomEventTypeParameterCheck(node);
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
