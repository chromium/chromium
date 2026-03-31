// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, extractClassImport, LIT_IMPORT_REGEX} from './query_utils.js';

export const litElementExpressions = ESLintUtils.RuleCreator.withoutDocs({
  name: 'lit-element-expressions',
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that expressions in a Lit element\'s template are not used with incompatible properties',
      recommended: 'error',
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
    },
  },
  defaultOptions: [],
  create(context) {
    function extractPropertiesFromClass(file, className) {
      const parser = context.languageOptions.parser;
      const parserOptions = context.languageOptions.parserOptions;
      const ast = parser.parse(file.text, {
        ...parserOptions,
        filePath: file.fileName,
      });
      // Match on class name suffix as well because some UIs use aliasing.
      const propertiesQuery = esquery.parse(`ClassDeclaration[id.name=/${
          className}/] > ClassBody > MethodDefinition[key.name="properties"] > FunctionExpression > BlockStatement > ReturnStatement > ObjectExpression > Property`);
      return esquery.match(ast, propertiesQuery);
    }

    // Property binding validation: check that the bound property's type
    // is compatible with |expressionType|.
    function checkPropertyBinding(
        currentTagName, propBinding, expression, tsNode, expressionType,
        expressionTypeStr, checker) {
      // Use the HTMLElementTagNameMap to get the class name from the
      // tag name.
      const mapSymbol = checker.resolveName(
          'HTMLElementTagNameMap', tsNode, ts.SymbolFlags.Interface,
          /* escapeGlobals= */ false);
      assert.ok(mapSymbol && mapSymbol.members);
      const elSymbol = mapSymbol.members.get(currentTagName);
      assert.ok(elSymbol);
      const elementType = checker.getTypeOfSymbolAtLocation(elSymbol, tsNode);
      const propSymbol = elementType.getProperty(propBinding);

      // Getting the symbol can fail for mixin inherited properties.
      if (!propSymbol) {
        return;
      }

      const expectedType =
          checker.getTypeOfSymbolAtLocation(propSymbol, tsNode);
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

    // Determine the type that is declared for the Lit reactive property,
    // for expressions of form "this.someProp". This can fail for reactive
    // properties that are inherited from mixins.
    function getReactivePropertyType(propName, declaredProps) {
      const declaredProp =
          declaredProps.find(prop => prop.key.name === propName);
      if (!declaredProp) {
        return null;
      }
      const declaredType =
          declaredProp.value.properties.find(prop => prop.key.name === 'type');
      return declaredType ? declaredType.value.name : null;
    }

    // If info for the class property and corresponding Lit reactive
    // property both exist, ensure the two match. Note: Not yet
    // checking this for numbers/strings, as the use of enum types
    // can complicate the check.
    function checkDeclaredTypeMatch(
        expression, declaredProps, propName, expressionTypeStr, isTsBoolean,
        isTsObjectOrArray) {
      const declaredTypeName = getReactivePropertyType(propName, declaredProps);
      if (!declaredTypeName) {
        // Early return instead of error because the property may be
        // inherited from a mixin.
        return;
      }

      const isBooleanType = declaredTypeName === 'Boolean';
      const isObjectOrArrayType =
          declaredTypeName === 'Array' || declaredTypeName === 'Object';

      if ((isBooleanType && !isTsBoolean) ||
          (isObjectOrArrayType && !isTsObjectOrArray)) {
        context.report({
          node: expression,
          messageId: 'propertyTypeMismatch',
          data: {
            propertyName: propName,
            declaredType: declaredTypeName,
            tsType: expressionTypeStr,
          },
        });
      }
    }

    function checkBooleanAttributeBinding(
        boolName, expression, isTsBoolean, propName) {
      // Check 1: Should not bind to "true" or "false" literal values.
      if (expression.type === 'Literal' &&
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
      if (isTsBoolean) {
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
        attrName, expression, isTsObjectOrArray, propName) {
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

    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    let hasLitImport = false;
    let className = '';
    let classDefinitionFile = null;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](node) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name="getHtml"]'](node) {
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
      ['FunctionDeclaration[id.name="getHtml"] TemplateLiteral'](node) {
        if (className === '' || classDefinitionFile === null) {
          return;
        }

        const declaredProps =
            extractPropertiesFromClass(classDefinitionFile, className);
        if (!declaredProps) {
          // Ignore seemingly missing properties, as these may come from mixins
          // compiled in separate libraries.
          return;
        }

        const bindingRegex =
            /(\s+(?<attrName>[a-z0-9\-]+)|\?(?<boolName>[a-z0-9-]+)|\.(?<propName>[a-zA-Z0-9-]+))="$/;
        let currentTagName = '';

        for (let i = 0; i < node.quasis.length; i++) {
          // Extract the last tag name that was seen before an expression
          // started. This is necessary because quasis and tags are not 1 to 1.
          const tagMatch =
              /<([a-zA-Z0-9-]+)[^>]*$/.exec(node.quasis[i].value.raw);
          if (tagMatch) {
            currentTagName = tagMatch[1];
          }

          const match = bindingRegex.exec(node.quasis[i].value.raw);
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

          // Validate property bindings
          const propBinding = match.groups['propName'];
          if (propBinding && currentTagName) {
            checkPropertyBinding(
                currentTagName, propBinding, expression, tsNode, expressionType,
                expressionTypeStr, checker);
            continue;
          }

          // Check for a TypeScript boolean or object/array type.
          const isTsBoolean = expressionTypeStr === 'boolean' ||
              expressionTypeStr === 'true' || expressionTypeStr === 'false';
          const isTsObjectOrArray =
              (expressionType.flags & ts.TypeFlags.Object) !== 0 ||
              expressionTypeStr.endsWith('[]') ||
              expressionTypeStr.startsWith('Array<') ||
              expressionTypeStr.startsWith('Record<') ||
              expressionTypeStr.startsWith('{') ||
              expressionTypeStr === 'object';

          // Determine if binding is to a reactive property.
          const isBindingToProperty = expression.type === 'MemberExpression' &&
              expression.object.type === 'ThisExpression';
          const propName = isBindingToProperty ?
              expression.property.name :
              context.sourceCode.getText(expression);

          // Check type declared in properties() is compatible with TS type.
          if (isBindingToProperty) {
            checkDeclaredTypeMatch(
                expression, declaredProps, propName, expressionTypeStr,
                isTsBoolean, isTsObjectOrArray);
          }

          // Boolean attribute binding validation
          const boolName = match.groups['boolName'];
          if (boolName) {
            checkBooleanAttributeBinding(
                boolName, expression, isTsBoolean, propName);
            continue;
          }

          // Generic attribute binding validation
          const attrName = match.groups['attrName'];
          if (attrName) {
            checkAttributeBindingForObjectsAndArrays(
                attrName, expression, isTsObjectOrArray, propName);
          }
        }
      },
    };
  },
});
