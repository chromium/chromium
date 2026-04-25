// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, extractClassImport, extractPropertiesFromClass, getLitPropertyType, isIdentifier, isLiteral, isType, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds = 'incorrectAttributeBinding'|'incorrectBooleanBinding'|
    'noTrueBinding'|'noFalseBinding'|'propertyTypeMismatch'|
    'bindingTypeMismatch'|'propertyNotFound'|'listenerNotCallable';

export const litElementExpressions = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that expressions in a Lit element\'s template are not used with incompatible properties',
    },
    messages: {
      incorrectAttributeBinding:
          'Incorrect assignment to property \'{{propertyName}}\' using attribute expression \'{{attributeExpression}}\'. Object/Array Lit properties can only be initialized with property expressions. Change to \'{{propertyExpression}}\' instead, or update the property\'s type if Object/Array is not accurate.',
      incorrectBooleanBinding:
          'Incorrect assignment to property \'{{propertyName}}\' using boolean attribute expression \'{{attributeExpression}}\'. Boolean attribute expressions should only be assigned to boolean properties. To bind to the truthiness of \'{{propertyName}}\', convert it to a boolean using \'!!\'.',
      noTrueBinding:
          'Boolean attribute \'{{attributeName}}\' does not need to be bound to \'${true}\'. Use either \'{{attributeName}}\' or \'.{{propertyName}}="${true}"\' instead.',
      noFalseBinding:
          'Incorrect assignment to boolean attribute expression \'?{{attributeName}}=\' using \'${false}\'. Use property binding \'.{{propertyName}}="${false}"\' instead.',
      propertyTypeMismatch:
          'Property type mismatch: {{propertyName}} is declared as {{declaredType}} reactive property but is typed as {{tsType}}.',
      bindingTypeMismatch:
          'Type mismatch in property binding: Property \'{{propertyName}}\' on element \'{{tagName}}\' expects type \'{{expectedType}}\', but was provided \'{{providedType}}\'.',
      propertyNotFound:
          'Property \'{{propertyName}}\' was not found on element \'{{tagName}}\'.',
      listenerNotCallable:
          'Event listener for \'@{{eventName}}\' must be callable.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    // Property binding validation: check that the bound property's type
    // is compatible with |expressionType|.
    function checkPropertyBinding(
        currentTagName: string, propBinding: string,
        expression: TSESTree.Expression, tsNode: ts.Node,
        expressionType: ts.Type, expressionTypeStr: string,
        checker: ts.TypeChecker) {
      // Use the HTMLElementTagNameMap to get the class name from the
      // tag name.
      const mapSymbol = checker.resolveName(
          'HTMLElementTagNameMap', tsNode, ts.SymbolFlags.Interface,
          /* escapeGlobals= */ false);
      assert.ok(mapSymbol && mapSymbol.members);
      const elSymbol =
          mapSymbol.members.get(currentTagName as ts.InternalSymbolName);
      assert.ok(elSymbol);
      const elementType = checker.getTypeOfSymbolAtLocation(elSymbol, tsNode);
      const apparentType = checker.getApparentType(elementType);

      const propSymbol = apparentType.getProperty(propBinding);
      const expectedType = propSymbol ?
          checker.getTypeOfSymbolAtLocation(propSymbol, tsNode) :
          null;

      // Non-existent property exception for cr-auto-img, TS compiler does not
      // know HTMLImageElement has an autoSrc property.
      if (!expectedType && propBinding === 'autoSrc' &&
          currentTagName === 'img') {
        return;
      }

      // Binding to non-existent property.
      if (!expectedType) {
        context.report({
          node: expression,
          messageId: 'propertyNotFound',
          data: {
            propertyName: propBinding,
            tagName: currentTagName,
          },
        });
        return;
      }

      if (checker.isTypeAssignableTo(expressionType, expectedType)) {
        return;
      }

      // Exception 1: Allow TrustedHTML to be assigned to string
      // properties (e.g. innerHTML) for compatibility with Chromium's
      // patch.
      if (checker.typeToString(expressionType) === 'TrustedHTML' &&
          checker.typeToString(expectedType) === 'string') {
        return;
      }

      // Exception 2: style expects a CSSStyleDeclaration. Allow a
      // string.
      if (checker.typeToString(expressionType) === 'string' &&
          checker.typeToString(expectedType) === 'CSSStyleDeclaration') {
        return;
      }

      // Exception 3: Lit's "nothing" symbol is allowed. Check if the
      // expression's text ends with "nothing" to handle cases like
      // "this.property ? 'val' : nothing".
      const expressionText = context.sourceCode.getText(expression);
      if (expressionText.endsWith('nothing')) {
        return;
      }

      context.report({
        node: expression,
        messageId: 'bindingTypeMismatch',
        data: {
          propertyName: propBinding,
          tagName: currentTagName,
          expectedType: checker.typeToString(expectedType),
          providedType: expressionTypeStr,
        },
      });
    }

    function checkBooleanAttributeBinding(
        boolName: string, expression: TSESTree.Expression,
        expressionTypeStr: string, propName: string) {
      // Check 1: Should not bind to "true" or "false" literal values.
      if (isLiteral(expression) &&
          (expression.value === true || expression.value === false)) {
        context.report({
          node: expression,
          messageId: expression.value ? 'noTrueBinding' : 'noFalseBinding',
          data: {
            attributeName: boolName,
            propertyName: dashCaseToCamelCase(boolName),
          },
        });
        return;
      }

      // Check 2: Should only bind to boolean TS expressions.
      if (expressionTypeStr === 'boolean' || expressionTypeStr === 'true' ||
          expressionTypeStr === 'false') {
        return;
      }

      context.report({
        node: expression,
        messageId: 'incorrectBooleanBinding',
        data: {
          attributeExpression: `?${boolName}=`,
          propertyName: propName,
        },
      });
    }

    // Ensure attribute bindings are never used to bind to objects/arrays.
    function checkAttributeBindingForObjectsAndArrays(
        attrName: string, expression: TSESTree.Expression,
        isTsObjectOrArray: boolean, propName: string) {
      if (!isTsObjectOrArray) {
        return;
      }

      context.report({
        node: expression,
        messageId: 'incorrectAttributeBinding',
        data: {
          attributeExpression: `${attrName}=`,
          propertyName: propName,
          propertyExpression: `.${dashCaseToCamelCase(attrName)}=`,
        },
      });
    }

    function checkEventListenerBinding(
        eventName: string, expression: TSESTree.Expression,
        expressionType: ts.Type, checker: ts.TypeChecker) {
      const expressionText = context.sourceCode.getText(expression);

      function isValidListenerType(type: ts.Type): boolean {
        // Exception for Lit's "nothing" symbol.
        if (expressionText.endsWith('nothing')) {
          return true;
        }

        return checker.getSignaturesOfType(type, ts.SignatureKind.Call).length >
            0;
      }

      if (expressionType.isUnion() &&
          expressionType.types.every(isValidListenerType)) {
        return;
      }

      if (!expressionType.isUnion() && isValidListenerType(expressionType)) {
        return;
      }

      context.report({
        node: expression,
        messageId: 'listenerNotCallable',
        data: {eventName},
      });
    }

    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    let hasLitImport = false;
    let className = '';
    let classDefinitionFile: ts.SourceFile|null = null;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](
          _node: TSESTree.ImportDeclaration) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name="getHtml"]'](
          node: TSESTree.FunctionDeclarationWithName) {
        if (!hasLitImport) {
          return;
        }

        const classImport = extractClassImport(node, context.sourceCode.ast);
        if (classImport.type === '') {
          // Handle a few cases where lit-html is used directly and there is no
          // classDefinitionFile.
          return;
        }

        classDefinitionFile =
            sourceFiles.find(
                f => path.basename(f.fileName) === classImport.fileName) ||
            null;
        className = classImport.type;
      },
      ['FunctionDeclaration[id.name="getHtml"] TemplateLiteral'](
          node: TSESTree.TemplateLiteral) {
        if (className === '' || classDefinitionFile === null) {
          return;
        }

        extractPropertiesFromClass(classDefinitionFile, className, context);

        const bindingRegex =
            /(\s+(?<attrName>[a-z0-9\-]+)|\?(?<boolName>[a-z0-9-]+)|\.(?<propName>[a-zA-Z0-9-]+)|@(?<eventName>[a-zA-Z0-9-]+))="$/;
        let currentTagName = '';

        for (let i = 0; i < node.quasis.length; i++) {
          // Extract the last tag name that was seen before an expression
          // started. This is necessary because quasis and tags are not 1 to 1.
          const tagMatch =
              /<([a-zA-Z0-9-]+)[^>]*$/.exec(node.quasis[i]!.value.raw);
          if (tagMatch) {
            currentTagName = tagMatch[1]!;
          }

          const match = bindingRegex.exec(node.quasis[i]!.value.raw);
          if (!match) {
            continue;
          }

          const expression = node.expressions[i];
          if (!expression) {
            continue;
          }

          // Determine the TypeScript type of the bound expression.
          const checker = services.program.getTypeChecker();
          const tsNode = services.esTreeNodeToTSNodeMap.get(expression);
          assert.ok(tsNode);
          const expressionType = checker.getTypeAtLocation(tsNode);
          const expressionTypeStr = checker.typeToString(expressionType);

          // Determine if binding is to a reactive property.
          const isBindingToProperty =
              isType(expression, Node.MemberExpression) &&
              isType(expression.object, Node.ThisExpression);

          let propName: string;
          if (isBindingToProperty) {
            const memberExpression = expression as TSESTree.MemberExpression;
            assert.ok(isIdentifier(memberExpression.property));
            propName = memberExpression.property.name;
          } else {
            propName = context.sourceCode.getText(expression);
          }

          // Validate property bindings
          assert.ok(match.groups);
          const propBinding = match.groups['propName'];
          if (propBinding && currentTagName) {
            checkPropertyBinding(
                currentTagName, propBinding, expression, tsNode, expressionType,
                expressionTypeStr, checker);
            continue;
          }

          // Boolean attribute binding validation
          const boolName = match.groups['boolName'];
          if (boolName) {
            checkBooleanAttributeBinding(
                boolName, expression, expressionTypeStr, propName);
            continue;
          }

          // Event listener binding validation
          const eventName = match.groups['eventName'];
          if (eventName) {
            checkEventListenerBinding(
                eventName, expression, expressionType, checker);
            continue;
          }

          // Generic attribute binding validation
          const attrName = match.groups['attrName'];
          if (attrName) {
            const litType = getLitPropertyType(expressionType, checker);
            checkAttributeBindingForObjectsAndArrays(
                attrName, expression,
                litType === 'Object' || litType === 'Array', propName);
          }
        }
      },
    };
  },
});
