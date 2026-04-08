// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/types/dist/index.js';
import {AST_NODE_TYPES} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

// NOTE: Using `\u002F` instead of a forward slash, to workaround for
// https://github.com/eslint/eslint/issues/16555,
// https://eslint.org/docs/latest/extend/selectors#known-issues
// where forward slashes are not properly escaped in regular expressions
// appearing in AST selectors.
export const LIT_IMPORT_REGEX =
    ['resources', 'lit', 'v3_0', 'lit.rollup.js$'].join('\\u002F');

export const CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR =
    'CallExpression[callee.name=/Mixin(Lit)?$/][arguments.0.name="CrLitElement"]';

export const POLYMER_ELEMENT_EXTENDS_MIXIN_SELECTOR =
    'CallExpression[callee.name=/Mixin(Polymer)?$/][arguments.0.name="PolymerElement"]';

export function isCrLitElementSubclass(
    node: TSESTree.ClassDeclaration, programNode: TSESTree.Program): boolean {
  assert.ok(node.type === AST_NODE_TYPES.ClassDeclaration);

  if (!node.superClass) {
    return false;
  }

  if (node.superClass.type === AST_NODE_TYPES.Identifier) {
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

  if (node.superClass.type === AST_NODE_TYPES.CallExpression) {
    // Case3: 'MyElement extends SomeMixin(SomeOtherMixin(CrLitElement)) {...}'
    const selector = esquery.parse(CR_LIT_ELEMENT_EXTENDS_MIXIN_SELECTOR);
    const matchingNodes = esquery.match(node.superClass, selector);
    return matchingNodes.length > 0;
  }

  return false;
}

export function isPolymerElementSubclass(
    node: TSESTree.ClassDeclaration, programNode: TSESTree.Program): boolean {
  assert.ok(node.type === AST_NODE_TYPES.ClassDeclaration);

  if (!node.superClass) {
    return false;
  }

  if (node.superClass.type === AST_NODE_TYPES.Identifier) {
    if (node.superClass.name === 'PolymerElement') {
      // Case1: 'MyElement extends PolymerElement {...}'
      return true;
    }

    // Case2:
    // const MyElementBase = SomeMixin(PolymerElement);
    // MyElement extends MyElementBase {...}'
    const baseClassSelector = esquery.parse(
        `Program > VariableDeclaration > VariableDeclarator[id.name="${
            node.superClass.name}"] ${POLYMER_ELEMENT_EXTENDS_MIXIN_SELECTOR}`);
    const matchingNodes = esquery.match(programNode, baseClassSelector);
    return matchingNodes.length > 0;
  }

  if (node.superClass.type === AST_NODE_TYPES.CallExpression) {
    // Case3: 'MyElement extends
    // SomeMixin(SomeOtherMixin(PolymerElement)) {...}'
    const selector = esquery.parse(POLYMER_ELEMENT_EXTENDS_MIXIN_SELECTOR);
    const matchingNodes = esquery.match(node.superClass, selector);
    return matchingNodes.length > 0;
  }

  return false;
}

export function isIdentifier(node: TSESTree.Node): node is TSESTree.Identifier {
  return node.type === AST_NODE_TYPES.Identifier;
}

export function isLiteral(node: TSESTree.Node): node is TSESTree.Literal {
  return node.type === AST_NODE_TYPES.Literal;
}

export function dashCaseToCamelCase(string: string): string {
  return string.replace(/-([a-z])/g, group => group[1]!.toUpperCase());
}

interface ClassImportInfo {
  // The type of the "this" parameter in the getHtml() method.
  type: string;
  // The name of the file the class type is imported from.
  fileName: string;
}

// Extracts information about the imported element class from a Lit element
// template file.
export function extractClassImport(
    node: TSESTree.FunctionDeclarationWithName,
    programNode: TSESTree.Program): ClassImportInfo {
  assert.ok(
      node.type === AST_NODE_TYPES.FunctionDeclaration &&
      node.id.name === 'getHtml');
  const paramSelector = esquery.parse('Identifier[name="this"]');
  const matchingNodes =
      esquery.match(node, paramSelector) as TSESTree.Identifier[];
  if (matchingNodes.length === 0) {
    // Case where there is no "this" parameter for the getHtml() method
    return {
      type: '',
      fileName: '',
    };
  }

  const typeReference = matchingNodes[0]!.typeAnnotation!.typeAnnotation as
      TSESTree.TSTypeReference;
  const type = (typeReference.typeName as TSESTree.Identifier).name;

  // Find the URL of the import that imports the class.
  const importNodeQuery = esquery.parse('ImportDeclaration');
  const importNodes = esquery.match(programNode, importNodeQuery) as
      TSESTree.ImportDeclaration[];

  const classImportNode = importNodes.find(importNode => {
    return importNode.specifiers.some(specifier => {
      return specifier.local.name === type;
    });
  });
  assert.ok(!!classImportNode);

  return {
    type: type,
    fileName: path.basename(classImportNode.source.value).replace('.js', '.ts'),
  };
}

function isArrayType(typeStr: string): boolean {
  return typeStr.endsWith('[]') || typeStr.startsWith('Array<');
}

function isObjectType(type: ts.Type, typeStr: string): boolean {
  if (isArrayType(typeStr)) {
    return false;
  }
  if ((type.flags & (ts.TypeFlags.Object | ts.TypeFlags.Intersection)) !== 0) {
    return true;
  }
  return typeStr.startsWith('Record<') || typeStr.startsWith('{') ||
      typeStr === 'object';
}

export function getLitPropertyType(
    type: ts.Type, checker: ts.TypeChecker): string|null {
  const nonNullableType = checker.getNonNullableType(type);
  const nonNullableTypeStr = checker.typeToString(nonNullableType);
  if (nonNullableTypeStr === 'TrustedHTML') {
    return 'String';
  }

  if (checker.isTypeAssignableTo(nonNullableType, checker.getBooleanType())) {
    return 'Boolean';
  }

  if (checker.isTypeAssignableTo(nonNullableType, checker.getStringType())) {
    return 'String';
  }

  if (checker.isTypeAssignableTo(nonNullableType, checker.getNumberType())) {
    return 'Number';
  }

  if (isArrayType(nonNullableTypeStr)) {
    return 'Array';
  }

  if (isObjectType(type, nonNullableTypeStr)) {
    return 'Object';
  }

  if ((type.flags & ts.TypeFlags.Union) !== 0) {
    const union = type as ts.UnionType;
    if (union.types.some(
            (t: ts.Type) => isObjectType(t, checker.typeToString(t)))) {
      return 'Object';
    }
  }

  return null;
}
