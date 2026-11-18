// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {camelCaseToDashCase, dashCaseToCamelCase, extractClassImport, extractPropertiesFromClass, getLitPropertyType, getSourceFiles, isIdentifier, isLiteral, isType, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds = 'bindingTypeMismatch'|'booleanPropertyAttributeBinding'|
    'camelCaseAttribute'|'incorrectAttributeBinding'|'incorrectBooleanBinding'|
    'invalidBooleanAttributeExpression'|'invalidPropertyExpression'|
    'listenerNotCallable'|'noFalseBinding'|'nonPublicPropertyBinding'|
    'noStaticTrueBooleanAttribute'|'noTrueBinding'|'propertyNotFound'|
    'propertyTypeMismatch';

// HTML enumerated attributes whose corresponding DOM IDL properties are typed
// as boolean in TypeScript's lib.dom.d.ts, but which require explicit "true" or
// "false" attribute values in HTML markup rather than boolean attribute syntax.
const ENUMERATED_BOOLEAN_IDL_ATTRIBUTES = new Set([
  'autocorrect',
  'draggable',
  'spellcheck',
  'translate',
]);

const OPEN_TAG_START_REGEX = /^<([a-zA-Z][a-zA-Z0-9-]*)/;
const ID_ATTRIBUTE_REGEX = /\bid="([^\s"]+)"/;
const STATIC_QUESTION_MARK_REGEX = /\s\?(?<boolName>[a-zA-Z0-9_-]+)/g;
const STATIC_TRUE_REGEX = /\s(?<attrName>[a-zA-Z0-9-]+)="true"/g;
const CAMEL_CASE_ATTR_REGEX =
    /\s(?<prefix>\??)(?<attrName>[a-z][a-zA-Z0-9-]*[A-Z][a-zA-Z0-9-]*)="/g;
const STATIC_PROPERTY_REGEX = /\s\.(?<propName>[a-zA-Z0-9_-]+)="/g;
const HTML_COMMENT_REGEX = /<!--[\s\S]*?-->/g;
const BINDING_REGEX =
    /(\?(?<boolName>[a-zA-Z0-9_-]+)|@(?<eventName>[a-zA-Z0-9-]+))="$|(\s+(?<attrName>[a-zA-Z0-9_-]+)|\.(?<propName>[a-zA-Z0-9_-]+))="(?<prefix>[^"]*)$/;

function getPropertyVisibility(propSymbol: ts.Symbol): 'private'|'protected'|
    'public' {
  const decl = propSymbol.valueDeclaration || propSymbol.declarations?.[0];
  if (!decl || !ts.canHaveModifiers(decl)) {
    return 'public';
  }
  const flags = ts.getCombinedModifierFlags(decl);
  if ((flags & ts.ModifierFlags.Private) !== 0) {
    return 'private';
  }
  if ((flags & ts.ModifierFlags.Protected) !== 0) {
    return 'protected';
  }
  return 'public';
}

interface OpenTagInfo {
  tagName: string;
  tagId: string;
  inQuote: boolean;
}

function getOpenTagAtOffset(
    raw: string, offset: number, prevOpenTag: OpenTagInfo|null): OpenTagInfo|
    null {
  let tagName = prevOpenTag ? prevOpenTag.tagName : '';
  let inheritedTagId = prevOpenTag ? prevOpenTag.tagId : '';
  let inQuote = prevOpenTag ? prevOpenTag.inQuote : false;
  let tagStartIndex = prevOpenTag ? 0 : -1;

  for (let i = 0; i < offset; i++) {
    const ch = raw[i]!;
    if (tagName !== '') {
      if (ch === '"') {
        inQuote = !inQuote;
      } else if (!inQuote && ch === '>') {
        tagName = '';
        inheritedTagId = '';
        tagStartIndex = -1;
      }
    } else if (ch === '<') {
      const match = OPEN_TAG_START_REGEX.exec(raw.slice(i));
      if (match) {
        tagName = match[1]!;
        inQuote = false;
        tagStartIndex = i;
        i += match[0].length - 1;
      }
    }
  }

  if (tagName === '') {
    return null;
  }

  let scanInQuote = inQuote;
  let tagEndIndex = raw.length;
  for (let i = offset; i < raw.length; i++) {
    const ch = raw[i]!;
    if (ch === '"') {
      scanInQuote = !scanInQuote;
    } else if (!scanInQuote && ch === '>') {
      tagEndIndex = i;
      break;
    }
  }

  const fullTagChunk = raw.slice(tagStartIndex, tagEndIndex);
  const idMatch = ID_ATTRIBUTE_REGEX.exec(fullTagChunk);
  return {
    tagName,
    tagId: idMatch ? idMatch[1]! : inheritedTagId,
    inQuote,
  };
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

function getAttributePropertySymbol(
    apparentType: ts.Type, attrName: string, tsNode: ts.Node,
    checker: ts.TypeChecker): ts.Symbol|undefined {
  const camelName = dashCaseToCamelCase(attrName);
  const propSymbol = apparentType.getProperty(camelName);
  if (propSymbol || attrName.includes('-')) {
    return propSymbol;
  }
  // Handle native HTML boolean attributes whose DOM property is camelCased
  // differently from a single-word attribute (e.g. readonly -> readOnly).
  const candidate = apparentType.getProperties().find(
      p => p.name.toLowerCase() === attrName.toLowerCase());
  if (candidate &&
      getLitPropertyType(
          checker.getTypeOfSymbolAtLocation(candidate, tsNode), checker) ===
          'Boolean') {
    return candidate;
  }
  return undefined;
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

  const propSymbol =
      getAttributePropertySymbol(apparentType, attrName, tsNode, checker);
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
          'Type mismatch in {{bindingType}} binding: Property \'{{propertyName}}\' on element \'{{tagName}}\' expects type \'{{expectedType}}\', but was provided \'{{providedType}}\'.',
      booleanPropertyAttributeBinding:
          'Boolean property \'{{propertyName}}\' on element \'{{tagName}}\' can\'t be bound using a plain Lit attribute expression \'{{attributeName}}="${...}"\'. Use either boolean attribute expression \'?{{attributeName}}="${...}"\' or property expression \'.{{propertyName}}="${...}"\' instead.',
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
      nonPublicPropertyBinding:
          'Cannot bind to \'{{visibility}}\' property \'{{propertyName}}\' on element \'{{tagName}}\'. Make it public instead.',
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

      // Non-existent property exception for cr-auto-img, TS compiler does not
      // know HTMLImageElement has an autoSrc property.
      if (!propSymbol && propBinding === 'autoSrc' &&
          currentTagName === 'img') {
        return;
      }

      // Binding to non-existent property.
      if (!propSymbol) {
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

      const visibility = getPropertyVisibility(propSymbol);
      if (visibility !== 'public') {
        context.report({
          node: expression,
          messageId: 'nonPublicPropertyBinding',
          data: {
            propertyName: propBinding,
            tagName: currentTagName,
            visibility,
          },
        });
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
          bindingType: 'property',
          propertyName: propBinding,
          tagName: currentTagName,
          expectedType: checker.typeToString(expectedType),
          providedType: expressionTypeStr,
        },
      });
    }

    function checkAttributePropertyVisibility(
        currentTagName: string, currentTagId: string, attrName: string,
        expression: TSESTree.Expression, tsNode: ts.Node,
        checker: ts.TypeChecker): boolean {
      if (attrName.startsWith('aria-') || attrName.startsWith('data-')) {
        return false;
      }
      const apparentType =
          getElementApparentType(currentTagName, currentTagId, tsNode, checker);
      if (!apparentType) {
        return false;
      }
      const propName = dashCaseToCamelCase(attrName);
      const propSymbol = apparentType.getProperty(propName);
      if (!propSymbol) {
        return false;
      }
      const visibility = getPropertyVisibility(propSymbol);
      if (visibility !== 'public') {
        context.report({
          node: expression,
          messageId: 'nonPublicPropertyBinding',
          data: {
            propertyName: propName,
            tagName: currentTagName,
            visibility,
          },
        });
        return true;
      }
      return false;
    }

    function checkBooleanAttributeBinding(
        currentTagName: string, currentTagId: string, boolName: string,
        expression: TSESTree.Expression, tsNode: ts.Node,
        expressionTypeStr: string, propName: string, checker: ts.TypeChecker) {
      if (checkAttributePropertyVisibility(
              currentTagName, currentTagId, boolName, expression, tsNode,
              checker)) {
        return;
      }

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
        prevOpenTag: OpenTagInfo|null, tsNode: ts.Node,
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
        const openTag =
            getOpenTagAtOffset(rawWithoutComments, match.index, prevOpenTag);
        if (!openTag || openTag.inQuote) {
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
        const openTag =
            getOpenTagAtOffset(rawWithoutComments, match.index, prevOpenTag);
        if (!openTag || openTag.inQuote) {
          continue;
        }
        if (isBooleanAttribute(
                openTag.tagName, openTag.tagId, attrName, tsNode, checker)) {
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
        const openTag =
            getOpenTagAtOffset(rawWithoutComments, match.index, prevOpenTag);
        if (!openTag || openTag.inQuote) {
          continue;
        }
        if (isCamelCaseProperty(
                openTag.tagName, openTag.tagId, attrName, tsNode, checker)) {
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
        const openTag =
            getOpenTagAtOffset(rawWithoutComments, match.index, prevOpenTag);
        if (!openTag || openTag.inQuote) {
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

    function checkAttributeBinding(
        currentTagName: string, currentTagId: string, attrName: string,
        expression: TSESTree.Expression, tsNode: ts.Node,
        expressionType: ts.Type, expressionTypeStr: string,
        isTsObjectOrArray: boolean, propName: string, checker: ts.TypeChecker) {
      if (checkAttributePropertyVisibility(
              currentTagName, currentTagId, attrName, expression, tsNode,
              checker)) {
        return;
      }

      // Check 1: Should not bind to object or array TS expressions.
      if (isTsObjectOrArray) {
        context.report({
          node: expression,
          messageId: 'incorrectAttributeBinding',
          data: {
            attributeExpression: `${attrName}=`,
            propertyName: propName,
            propertyExpression: `.${dashCaseToCamelCase(attrName)}=`,
          },
        });
        return;
      }

      if (attrName !== attrName.toLowerCase() || attrName.startsWith('aria-') ||
          attrName.startsWith('data-') ||
          ENUMERATED_BOOLEAN_IDL_ATTRIBUTES.has(attrName)) {
        return;
      }

      const apparentType =
          getElementApparentType(currentTagName, currentTagId, tsNode, checker);
      if (!apparentType) {
        return;
      }

      const propSymbol =
          getAttributePropertySymbol(apparentType, attrName, tsNode, checker);
      if (!propSymbol) {
        return;
      }

      const targetPropName = propSymbol.name;
      const expectedType =
          checker.getTypeOfSymbolAtLocation(propSymbol, tsNode);
      const expectedTypeStr = checker.typeToString(expectedType);
      if (expectedTypeStr === 'CSSStyleDeclaration' ||
          expectedTypeStr === 'DOMTokenList') {
        return;
      }

      const targetLitType = getLitPropertyType(expectedType, checker);

      // Check 2: Should not bind to an Object or Array property on the element.
      if (targetLitType === 'Object' || targetLitType === 'Array') {
        context.report({
          node: expression,
          messageId: 'incorrectAttributeBinding',
          data: {
            attributeExpression: `${attrName}=`,
            propertyName: targetPropName,
            propertyExpression: `.${targetPropName}=`,
          },
        });
        return;
      }

      // Check 3: Should not bind to a Boolean property on the element using
      // attribute expression syntax.
      if (targetLitType === 'Boolean') {
        context.report({
          node: expression,
          messageId: 'booleanPropertyAttributeBinding',
          data: {
            attributeName: attrName,
            propertyName: targetPropName,
            tagName: currentTagName,
          },
        });
        return;
      }

      // Check 4: Ensure bindings to a Number property on the element are
      // assignable to the property's type.
      if (targetLitType === 'Number') {
        const expressionText = context.sourceCode.getText(expression);
        if (expressionText.endsWith('nothing')) {
          return;
        }

        if (!checker.isTypeAssignableTo(expressionType, expectedType)) {
          context.report({
            node: expression,
            messageId: 'bindingTypeMismatch',
            data: {
              bindingType: 'attribute',
              propertyName: targetPropName,
              tagName: currentTagName,
              expectedType: checker.typeToString(expectedType),
              providedType: expressionTypeStr,
            },
          });
        }
      }
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
    const sourceFiles = getSourceFiles(context);

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
        let currentOpenTag: OpenTagInfo|null = null;

        for (let i = 0; i < node.quasis.length; i++) {
          const quasi = node.quasis[i]!;
          const rawWithoutComments =
              quasi.value.raw.replace(HTML_COMMENT_REGEX, '');

          checkStaticAttributes(
              quasi, rawWithoutComments, currentOpenTag, templateTsNode,
              checker);

          // Update the enclosing unclosed opening `<tag ...>` (if any) before
          // `node.expressions[i]`. When `currentOpenTag` is null (e.g. child
          // text like `<div>${this.x}</div>` where `<div>` already closed with
          // `>`, or the trailing tail quasi) or not inside a quoted attribute,
          // the expression cannot be an attribute, property, or event binding.
          currentOpenTag = getOpenTagAtOffset(
              rawWithoutComments, rawWithoutComments.length, currentOpenTag);
          if (!currentOpenTag || !currentOpenTag.inQuote) {
            continue;
          }

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
          const hasStaticPrefix = (match.groups['prefix'] ?? '') !== '';
          const hasStaticSuffix =
              !node.quasis[i + 1]?.value.raw.startsWith('"');
          const isConcatenatedString = hasStaticPrefix || hasStaticSuffix;

          const propBinding = match.groups['propName'];
          if (propBinding) {
            checkPropertyBinding(
                currentOpenTag.tagName, currentOpenTag.tagId, propBinding,
                expression, tsNode,
                isConcatenatedString ? checker.getStringType() : expressionType,
                isConcatenatedString ? 'string' : expressionTypeStr, checker);
            continue;
          }

          // Boolean attribute binding validation
          const boolName = match.groups['boolName'];
          if (boolName) {
            checkBooleanAttributeBinding(
                currentOpenTag.tagName, currentOpenTag.tagId, boolName,
                expression, tsNode, expressionTypeStr, propName, checker);
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
            checkAttributeBinding(
                currentOpenTag.tagName, currentOpenTag.tagId, attrName,
                expression, tsNode,
                isConcatenatedString ? checker.getStringType() : expressionType,
                isConcatenatedString ? 'string' : expressionTypeStr,
                litType === 'Object' || litType === 'Array', propName, checker);
          }
        }
      },
    };
  },
});
