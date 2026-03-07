// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
import path from 'node:path';

import {ESLintUtils} from '../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '../../../../../third_party/node/node_modules/esquery/dist/esquery.esm.min.js';

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

          if (node.expressions[i].type !== 'MemberExpression') {
            continue;
          }

          const propName = node.expressions[i].property.name;
          const declaredProp =
              declaredProps.find(prop => prop.key.name === propName);
          if (!declaredProp) {
            // Ignore seemingly missing properties, as these may be from mixins.
            continue;
          }

          const declaredType = declaredProp.value.properties.find(
              prop => prop.key.name === 'type');
          assert.ok(!!declaredType);
          if (!!match.groups['boolName'] &&
              declaredType.value.name !== 'Boolean') {
            context.report({
              node: node.expressions[i],
              messageId: 'incorrectBooleanBinding',
              data: {
                attributeExpression: `?${match.groups['boolName']}=`,
                propertyName: propName,
              },
            });
            continue;
          }

          if (declaredType.value.name === 'Array' ||
              declaredType.value.name === 'Object') {
            const attrName = match.groups['attrName'];
            context.report({
              node: node.expressions[i],
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
