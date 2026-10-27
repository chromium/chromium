// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {camelCaseToDashCase, dashCaseToCamelCase, extractClassImport, extractPropertiesFromClass, getLitPropertyType, isIdentifier, isLiteral, isType, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds = 'bindingTypeMismatch'|'camelCaseAttribute'|
    'incorrectAttributeBinding'|'incorrectBooleanBinding'|
    'invalidBooleanAttributeExpression'|'invalidPropertyExpression'|
    'listenerNotCallable'|'noFalseBinding'|'noStaticTrueBooleanAttribute'|
    'noTrueBinding'|'propertyNotFound'|'propertyTypeMismatch';

// HTML enumerated attributes whose corresponding DOM IDL properties are typed
// as boolean in TypeScript's lib.dom.d.ts, but which require explicit "true" or
// "false" attribute values in HTML markup rather than boolean attribute syntax.
const ENUMERATED_BOOLEAN_IDL_ATTRIBUTES = new Set([
  'autocorrect',
  'draggable',
  'spellcheck',
  'translate',
]);

const LOCAL_TAG_REGEX = /<([a-zA-Z0-9-]+)[^>]*$/;
const ID_ATTRIBUTE_REGEX = /\bid="([^\s"]+)"/;
const STATIC_QUESTION_MARK_REGEX = /\s\?(?<boolName>[a-zA-Z0-9-]+)/g;
const STATIC_TRUE_REGEX = /\s(?<attrName>[a-zA-Z0-9-]+)="true"/g;
const CAMEL_CASE_ATTR_REGEX =
    /\s(?<prefix>\??)(?<attrName>[a-z][a-zA-Z0-9-]*[A-Z][a-zA-Z0-9-]*)="/g;
const STATIC_PROPERTY_REGEX = /\s\.(?<propName>[a-zA-Z0-9_-]+)="/g;
const HTML_COMMENT_REGEX = /<!--[\s\S]*?-->/g;
const BINDING_REGEX =
    /(\s+(?<attrName>[a-zA-Z0-9\-]+)|\?(?<boolName>[a-zA-Z0-9-]+)|@(?<eventName>[a-zA-Z0-9-]+))="$|\.(?<propName>[a-zA-Z0-9_-]+)="(?<propPrefix>[^"]*)$/;

interface TagInfo {
  tagName: string;
  tagId: string;
}

function getTagAtOffset(
    raw: string, offset: number, prevTagName: string,
    prevTagId: string): TagInfo|null {
  const before = raw.slice(0, offset);
  const afterUntilClose = raw.slice(offset).split('>')[0]!;
  const localTagMatch = LOCAL_TAG_REGEX.exec(before);
  if (localTagMatch) {
    const fullTagChunk = localTagMatch[0] + afterUntilClose;
    const idMatch = ID_ATTRIBUTE_REGEX.exec(fullTagChunk);
    return {
      tagName: localTagMatch[1]!,
      tagId: idMatch ? idMatch[1]! : '',
    };
  }
  if (prevTagName !== '' && !before.includes('>') && !before.includes('<')) {
    const fullTagChunk = before + afterUntilClose;
    const idMatch = ID_ATTRIBUTE_REGEX.exec(fullTagChunk);
    return {
      tagName: prevTagName,
      tagId: idMatch ? idMatch[1]! : prevTagId,
    };
  }
  return null;
}

function getElementApparentType(
    currentTagName: string, currentTagId: string, tsNode: ts.Node,
    checker: ts.TypeChecker): ts.Type|null {
  let elSymbol: ts.Symbol|undefined;

  if (currentTagId !== '') {
    // 1. Check for a context-specific TemplatizedDomNodes interface or type
    // in the template file.
    const templatizedSymbol = checker.resolveName(
        'TemplatizedDomNodes', tsNode,
        ts.SymbolFlags.Interface | ts.SymbolFlags.TypeAlias,
        /* escapeGlobals= */ false);

    if (templatizedSymbol) {
      const templatizedType =
          checker.getDeclaredTypeOfSymbol(templatizedSymbol);
      elSymbol = templatizedType.getProperty(currentTagId);
    }
  }

  // 2. Fallback to HTMLElementTagNameMap if no context-specific type was
  // found.
  if (!elSymbol) {
    const mapSymbol = checker.resolveName(
        'HTMLElementTagNameMap', tsNode, ts.SymbolFlags.Interface,
        /* escapeGlobals= */ false);
    assert.ok(mapSymbol && mapSymbol.members);
    elSymbol = mapSymbol.members.get(currentTagName as ts.InternalSymbolName);
  }

  if (!elSymbol) {
    return null;
  }
  const elementType = checker.getTypeOfSymbolAtLocation(elSymbol, tsNode);
  return checker.getApparentType(elementType);
}

function isBooleanAttribute(
    currentTagName: string, currentTagId: string, attrName: string,
    tsNode: ts.Node, checker: ts.TypeChecker): boolean {
  if (attrName.startsWith('aria-') || attrName.startsWith('data-') ||
      ENUMERATED_BOOLEAN_IDL_ATTRIBUTES.has(attrName)) {
    return false;
  }

  const apparentType =
      getElementApparentType(currentTagName, currentTagId, tsNode, checker);
  if (!apparentType) {
    return false;
  }

  const camelName = dashCaseToCamelCase(attrName);
  let propSymbol = apparentType.getProperty(camelName);
  if (!propSymbol && !attrName.includes('-')) {
    // Handle native HTML boolean attributes whose DOM property is camelCased
    // differently from a single-word attribute (e.g. readonly -> readOnly).
    propSymbol = apparentType.getProperties().find(
        p => p.name.toLowerCase() === attrName.toLowerCase());
  }
  if (!propSymbol) {
    return false;
  }

  const propType = checker.getTypeOfSymbolAtLocation(propSymbol, tsNode);
  return getLitPropertyType(propType, checker) === 'Boolean';
}

function isCamelCaseProperty(
    currentTagName: string, currentTagId: string, attrName: string,
    tsNode: ts.Node, checker: ts.TypeChecker): boolean {
  const apparentType =
      getElementApparentType(currentTagName, currentTagId, tsNode, checker);
  if (!apparentType) {
    return false;
  }
  return Boolean(apparentType.getProperty(attrName));
}

export const litElementExpressions = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that expressions in a Lit element\'s template are not used with incompatible properties',
    },
    messages: {
      bindingTypeMismatch:
          'Type mismatch in property binding: Property \'{{propertyName}}\' on element \'{{tagName}}\' expects type \'{{expectedType}}\', but was provided \'{{providedType}}\'.',
      camelCaseAttribute:
          'Attribute \'{{attributeName}}\' uses camelCase. Use either \'.{{propertyName}}="${...}"\' or \'{{dashCaseName}}\' instead.',
      incorrectAttributeBinding:
          'Incorrect assignment to property \'{{propertyName}}\' using attribute expression \'{{attributeExpression}}\'. Object/Array Lit properties can only be initialized with property expressions. Change to \'{{propertyExpression}}\' instead, or update the property\'s type if Object/Array is not accurate.',
      incorrectBooleanBinding:
          'Incorrect assignment to property \'{{propertyName}}\' using boolean attribute expression \'{{attributeExpression}}\'. Boolean attribute expressions should only be assigned to boolean properties. To bind to the truthiness of \'{{propertyName}}\', convert it to a boolean using \'!!\'.',
      invalidBooleanAttributeExpression:
          'Invalid boolean attribute expression: \'?{{attributeName}}=\' syntax only works with \'${...}\' rvalues. Use just \'{{dashCaseName}}\' if always `true` or \'?{{dashCaseName}}="${...}"\' for a dynamic binding.',
      invalidPropertyExpression:
          'Invalid property expression: \'.{{propertyName}}=\' only works with \'${...}\' rvalues. Use attribute expression \'{{dashCaseName}}="..."\' for a static value, or \'.{{propertyName}}="${...}"\' for a dynamic binding.',
      listenerNotCallable:
          'Event listener for \'@{{eventName}}\' must be callable.',
      noFalseBinding:
          'Incorrect assignment to boolean attribute expression \'?{{attributeName}}=\' using \'${false}\'. Use property binding \'.{{propertyName}}="${false}"\' instead.',
      noStaticTrueBooleanAttribute:
          'Boolean attribute \'{{attributeName}}\' does not need to be set to \'="true"\'. Use either \'{{dashCaseName}}\' or \'.{{propertyName}}="${true}"\' instead.',
      noTrueBinding:
          'Boolean attribute \'{{attributeName}}\' does not need to be bound to \'${true}\'. Use either \'{{attributeName}}\' or \'.{{propertyName}}="${true}"\' instead.',
      propertyNotFound:
          'Property \'{{propertyName}}\' was not found on element \'{{tagName}}\'.',
      propertyTypeMismatch:
          'Property type mismatch: \'{{propertyName}}\' is declared as \'{{declaredType}}\' reactive property but is typed as \'{{tsType}}\'.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    // Property binding validation: check that the bound property's type
    // is compatible with |expressionType|.
    function checkPropertyBinding(
        currentTagName: string, currentTagId: string, propBinding: string,
        expression: TSESTree.Expression, tsNode: ts.Node,
        expressionType: ts.Type, expressionTypeStr: string,
        checker: ts.TypeChecker) {
      const apparentType =
          getElementApparentType(currentTagName, currentTagId, tsNode, checker);
      assert.ok(apparentType);

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
      if ((expressionTypeStr === 'TrustedHTML' ||
           expressionTypeStr === 'string | TrustedHTML') &&
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

    function checkStaticAttributes(
        quasi: TSESTree.TemplateElement, rawWithoutComments: string,
        prevTagName: string, prevTagId: string, tsNode: ts.Node,
        checker: ts.TypeChecker) {
      // Check 1: ?attr attributes not followed by ="${...}".
      for (const match of rawWithoutComments.matchAll(
               STATIC_QUESTION_MARK_REGEX)) {
        assert.ok(match.groups);
        const boolName = match.groups['boolName']!;
        const afterMatch =
            rawWithoutComments.slice(match.index + match[0].length);
        if (!quasi.tail && afterMatch === '="') {
          // Dynamic boolean attribute binding ?attr="${...}".
          continue;
        }
        const tagInfo = getTagAtOffset(
            rawWithoutComments, match.index, prevTagName, prevTagId);
        if (!tagInfo) {
          continue;
        }
        context.report({
          node: quasi,
          messageId: 'invalidBooleanAttributeExpression',
          data: {
            attributeName: boolName,
            dashCaseName: camelCaseToDashCase(boolName),
          },
        });
      }

      // Check 2: Static ="true" on boolean attributes.
      for (const match of rawWithoutComments.matchAll(STATIC_TRUE_REGEX)) {
        assert.ok(match.groups);
        const attrName = match.groups['attrName']!;
        const tagInfo = getTagAtOffset(
            rawWithoutComments, match.index, prevTagName, prevTagId);
        if (!tagInfo) {
          continue;
        }
        if (isBooleanAttribute(
                tagInfo.tagName, tagInfo.tagId, attrName, tsNode, checker)) {
          context.report({
            node: quasi,
            messageId: 'noStaticTrueBooleanAttribute',
            data: {
              attributeName: attrName,
              dashCaseName: camelCaseToDashCase(attrName),
              propertyName: dashCaseToCamelCase(attrName),
            },
          });
        }
      }

      // Check 3: CamelCase attributes matching a property on the element.
      for (const match of rawWithoutComments.matchAll(CAMEL_CASE_ATTR_REGEX)) {
        assert.ok(match.groups);
        const prefix = match.groups['prefix']!;
        const attrName = match.groups['attrName']!;
        const tagInfo = getTagAtOffset(
            rawWithoutComments, match.index, prevTagName, prevTagId);
        if (!tagInfo) {
          continue;
        }
        if (isCamelCaseProperty(
                tagInfo.tagName, tagInfo.tagId, attrName, tsNode, checker)) {
          context.report({
            node: quasi,
            messageId: 'camelCaseAttribute',
            data: {
              attributeName: `${prefix}${attrName}`,
              propertyName: attrName,
              dashCaseName: `${prefix}${camelCaseToDashCase(attrName)}`,
            },
          });
        }
      }

      // Check 4: Static .prop="..." attributes not bound to a ${...}
      // expression.
      for (const match of rawWithoutComments.matchAll(STATIC_PROPERTY_REGEX)) {
        assert.ok(match.groups);
        const propName = match.groups['propName']!;
        const afterMatch =
            rawWithoutComments.slice(match.index + match[0].length);
        if (!quasi.tail && !afterMatch.includes('"')) {
          // Dynamic property binding .prop="...${...}...".
          continue;
        }
        const tagInfo = getTagAtOffset(
            rawWithoutComments, match.index, prevTagName, prevTagId);
        if (!tagInfo) {
          continue;
        }
        context.report({
          node: quasi,
          messageId: 'invalidPropertyExpression',
          data: {
            propertyName: propName,
            dashCaseName: camelCaseToDashCase(propName),
          },
        });
      }
    }

    // Ensure attribute bindings do not bind to objects/arrays.
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

        const checker = services.program.getTypeChecker();
        const templateTsNode = services.esTreeNodeToTSNodeMap.get(node);
        assert.ok(templateTsNode);
        let currentTagName = '';
        let currentTagId = '';

        for (let i = 0; i < node.quasis.length; i++) {
          const quasi = node.quasis[i]!;
          const rawWithoutComments =
              quasi.value.raw.replace(HTML_COMMENT_REGEX, '');

          checkStaticAttributes(
              quasi, rawWithoutComments, currentTagName, currentTagId,
              templateTsNode, checker);

          // Extract the last tag name that was seen before an expression
          // started. This is necessary because quasis and tags are not 1 to 1.
          const endTagInfo = getTagAtOffset(
              rawWithoutComments, rawWithoutComments.length, currentTagName,
              currentTagId);
          currentTagName = endTagInfo ? endTagInfo.tagName : '';
          currentTagId = endTagInfo ? endTagInfo.tagId : '';

          const match = BINDING_REGEX.exec(rawWithoutComments);
          if (!match) {
            continue;
          }

          const expression = node.expressions[i];
          if (!expression) {
            continue;
          }

          // Determine the TypeScript type of the bound expression.
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
          if (propBinding && currentTagName !== '') {
            const hasStaticPrefix = match.groups['propPrefix'] !== '';
            const hasStaticSuffix =
                !node.quasis[i + 1]?.value.raw.startsWith('"');
            const isConcatenatedString = hasStaticPrefix || hasStaticSuffix;
            checkPropertyBinding(
                currentTagName, currentTagId, propBinding, expression, tsNode,
                isConcatenatedString ? checker.getStringType() : expressionType,
                isConcatenatedString ? 'string' : expressionTypeStr, checker);
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
