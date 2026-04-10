// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {extractClassImport, extractPropertiesFromClass, isIdentifier, isType, LIT_IMPORT_REGEX} from './query_utils.js';

type Options = [];
type MessageIds = 'missingLitProperty'|'getterInLitTemplate';

function isFunctionType(t: ts.Type): boolean {
  if (t.isUnion()) {
    return t.types.some(subType => subType.getCallSignatures().length > 0);
  }
  return t.getCallSignatures().length > 0;
}

export const litReactiveProperties = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that properties referenced in a Lit element\'s template are declared as reactive properties',
    },
    messages: {
      missingLitProperty:
          'Missing Lit reactive property declaration for property \'{{propertyName}}\' in class \'{{className}}\'.',
      getterInLitTemplate:
          '\'get\' syntax in Lit HTML templates is disallowed, encountered a getter for \'{{propertyName}}\' in class \'{{className}}\'.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    let classDefinitionFile: ts.SourceFile|null = null;
    let className = '';
    let localDeclaredProps: TSESTree.Property[]|null = null;
    let thisTypeForClass: ts.Type|null = null;

    // Checks if `propName` is in the Lit reactive properties.
    function checkMissingLitProperty(
        expression: TSESTree.Node, propName: string, className: string,
        thisType: ts.Type, checker: ts.TypeChecker,
        localDeclaredProps: TSESTree.Property[]) {
      const tsNode = services.esTreeNodeToTSNodeMap.get(expression);
      assert.ok(tsNode);
      const type = checker.getTypeAtLocation(tsNode);

      // Skip functions, which should not be reactive properties.
      if (isFunctionType(type)) {
        return;
      }

      // Walk up any parent nodes to catch methods on properties, e.g.
      // this.someObject.someMethod. These are used by some Lit elements to
      // attach to event handlers, and should not trigger a "missing reactive
      // property" error for `someObject`.
      let currentAstNode = expression;
      while (currentAstNode.parent) {
        if (isType(currentAstNode.parent, Node.MemberExpression) &&
            currentAstNode.parent.object === currentAstNode) {
          currentAstNode = currentAstNode.parent;
        } else if (
            isType(currentAstNode.parent, Node.ChainExpression) &&
            currentAstNode.parent.expression === currentAstNode) {
          // Handles optional chaining operator case, i.e.
          // this.someObject?.someMethod
          currentAstNode = currentAstNode.parent;
        } else {
          break;
        }

        const currentTsNode =
            services.esTreeNodeToTSNodeMap.get(currentAstNode);
        assert.ok(currentTsNode);
        const currentType = checker.getTypeAtLocation(currentTsNode);

        if (isFunctionType(currentType)) {
          return;
        }
      }

      const propSymbol = thisType.getProperty(propName);
      assert.ok(propSymbol);

      // Get declaration of property.
      const declarations = propSymbol.getDeclarations();
      assert.ok(declarations && declarations.length > 0);

      // Skip properties declared in other files (e.g., properties inherited
      // from mixins), as we don't always have access to the full AST.
      const declarationFile = declarations[0]!.getSourceFile().fileName;
      if (declarationFile !== classDefinitionFile!.fileName) {
        return;
      }

      const hasProp = localDeclaredProps.some(p => {
        if (isIdentifier(p.key)) {
          return p.key.name === propName;
        }
        return false;
      });

      if (!hasProp) {
        const isGetter =
            declarations.some(d => d.kind === ts.SyntaxKind.GetAccessor);
        context.report({
          node: expression,
          messageId: isGetter ? 'getterInLitTemplate' : 'missingLitProperty',
          data: {
            propertyName: propName,
            className,
          },
        });
      }
    }

    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();
    const checker = services.program.getTypeChecker();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    let hasLitImport = false;

    return {
      [`ImportDeclaration[source.value=/${LIT_IMPORT_REGEX}/]`](
          _node: TSESTree.ImportDeclaration) {
        hasLitImport = true;
      },
      ['FunctionDeclaration[id.name="getHtml"]'](
          node: TSESTree.FunctionDeclarationWithName) {
        // Reset state
        classDefinitionFile = null;
        className = '';
        localDeclaredProps = null;
        thisTypeForClass = null;

        if (!hasLitImport) {
          return;
        }

        const classImport = extractClassImport(node, context.sourceCode.ast);
        if (classImport.type === '') {
          return;
        }

        classDefinitionFile =
            sourceFiles.find(
                f => path.basename(f.fileName) === classImport.fileName) ||
            null;
        assert.ok(classDefinitionFile);
        className = classImport.type;

        // Get the reactive properties from the "this" parameter. The parameter
        // is guaranteed to exist since classImport.type !== ''.
        const paramSelector = esquery.parse('Identifier[name="this"]');
        const matchingNodes =
            esquery.match(node, paramSelector) as TSESTree.Identifier[];
        assert.ok(matchingNodes.length > 0);
        const thisTsNode =
            services.esTreeNodeToTSNodeMap.get(matchingNodes[0]!);
        assert.ok(thisTsNode);
        thisTypeForClass = checker.getTypeAtLocation(thisTsNode);
        assert.ok(thisTypeForClass);
        localDeclaredProps =
            extractPropertiesFromClass(classDefinitionFile, className, context);
      },
      ['FunctionDeclaration[id.name="getHtml"] TemplateLiteral'](
          node: TSESTree.TemplateLiteral) {
        if (localDeclaredProps === null) {
          return;
        }
        assert.ok(thisTypeForClass);

        const thisQuery = esquery.parse(
            'MemberExpression[object.type="ThisExpression"], ' +
            'MemberExpression[object.type="Identifier"][object.name="this"]');

        for (let i = 0; i < node.expressions.length; i++) {
          const expression = node.expressions[i]!;

          const thisExpressions = esquery.match(expression, thisQuery) as
              TSESTree.MemberExpression[];

          for (const e of thisExpressions) {
            assert.ok(isIdentifier(e.property));
            checkMissingLitProperty(
                e, e.property.name, className, thisTypeForClass, checker,
                localDeclaredProps);
          }
        }
      },
    };
  },
});
