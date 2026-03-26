// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import path from 'node:path';

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '../../../../../third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import ts from '../../../../../third_party/node/node_modules/typescript/lib/typescript.js';

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

        const attrBindingRegex =
            /(\s+(?<attrName>[a-z0-9\-]+)|\?(?<boolName>[a-z0-9-]+))="$/;
        for (let i = 0; i < node.quasis.length; i++) {
          const match = attrBindingRegex.exec(node.quasis[i].value.raw);
          if (!match) {
            continue;
          }

          const expression = node.expressions[i];
          if (!expression) {
            continue;
          }

          if (expression.type === 'Literal' && !!match.groups['boolName'] &&
              (expression.value === true || expression.value === false)) {
            const boolName = match.groups['boolName'];
            context.report({
              node: expression,
              messageId: expression.value ? 'noTrueBinding' : 'noFalseBinding',
              data: {
                attributeName: boolName,
                propertyName: dashCaseToCamelCase(boolName),
              },
            });
            continue;
          }

          const isPropertyBinding = expression.type === 'MemberExpression' &&
              expression.object.type === 'ThisExpression';
          const propName = isPropertyBinding ?
              expression.property.name :
              context.sourceCode.getText(expression);
          let isBooleanType = false;
          let isObjectOrArrayType = false;
          let declaredTypeName = null;

          // Determine the type that is declared for the Lit reactive property,
          // for expressions of form "this.someProp". This can fail for reactive
          // properties that are inherited from mixins.
          if (isPropertyBinding) {
            const declaredProp =
                declaredProps.find(prop => prop.key.name === propName);
            if (declaredProp) {
              const declaredType = declaredProp.value.properties.find(
                  prop => prop.key.name === 'type');
              if (declaredType) {
                declaredTypeName = declaredType.value.name;
                isBooleanType = declaredTypeName === 'Boolean';
                isObjectOrArrayType = declaredTypeName === 'Array' ||
                    declaredTypeName === 'Object';
              }
            }
          }

          // Determine the TypeScript type.
          const checker = services.program.getTypeChecker();
          const tsNode = services.esTreeNodeToTSNodeMap.get(expression);
          assert.ok(tsNode);
          const type = checker.getTypeAtLocation(tsNode);
          // Convert to non-nullable type for purposes of matching for now.
          // Optionally undefined types are commonly used for code migrated
          // from Polymer that relies on a parent passing a value via data
          // binding.
          const typeStr =
              checker.typeToString(checker.getNonNullableType(type));
          const isTsBoolean = typeStr === 'boolean' || typeStr === 'true' ||
              typeStr === 'false';
          const isTsObjectOrArray = (type.flags & ts.TypeFlags.Object) !== 0 ||
              typeStr.endsWith('[]') || typeStr.startsWith('Array<') ||
              typeStr.startsWith('Record<') || typeStr.startsWith('{') ||
              typeStr === 'object';

          if (declaredTypeName) {
            // If info for the class property and corresponding Lit reactive
            // property both exist, ensure the two match.
            if ((isBooleanType && !isTsBoolean) ||
                (isObjectOrArrayType && !isTsObjectOrArray)) {
              context.report({
                node: expression,
                messageId: 'propertyTypeMismatch',
                data: {
                  propertyName: propName,
                  declaredType: declaredTypeName,
                  tsType: typeStr,
                },
              });
              continue;
            }
          } else {
            // Fall back to TS type if a declared type was not found for the
            // reactive property.
            isBooleanType = isTsBoolean;
            isObjectOrArrayType = isTsObjectOrArray;
          }

          if (!!match.groups['boolName'] && !isBooleanType) {
            context.report({
              node: expression,
              messageId: 'incorrectBooleanBinding',
              data: {
                attributeExpression: `?${match.groups['boolName']}=`,
                propertyName: propName,
              },
            });
            continue;
          }

          if (!!match.groups['attrName'] && isObjectOrArrayType) {
            const attrName = match.groups['attrName'];
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
        }
      },
    };
  },
});
