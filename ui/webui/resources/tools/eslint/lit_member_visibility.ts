// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import assert from 'node:assert';

import {extractTemplatesFromFile, getSourceFiles, isCrLitElementSubclass, isIdentifier, isLiteral, isMixinClass, isType, PROPERTIES_METHOD_PROPERTY_SELECTOR, THIS_MEMBER_EXPRESSION_QUERY} from './query_utils.js';

type Options = [];
type MessageIds = 'unnecessaryProtectedProperty';

class ClassInfo {
  private readonly context: TSESLint.RuleContext<MessageIds, Options>;
  private readonly name: string;
  readonly protectedProperties:
      Array<TSESTree.PropertyDefinition|TSESTree.AccessorProperty> = [];
  readonly reflectedProperties: Set<string> = new Set();
  inlinedRenderBlock: TSESTree.BlockStatement|null = null;
  private readonly templateReferencedProperties: Set<string> = new Set();

  constructor(
      context: TSESLint.RuleContext<MessageIds, Options>, name: string) {
    this.context = context;
    this.name = name;
  }

  visitTemplateBlock(templateBlock: TSESTree.BlockStatement) {
    const thisExpressions =
        esquery.match(templateBlock, THIS_MEMBER_EXPRESSION_QUERY) as
        TSESTree.MemberExpression[];
    for (const expr of thisExpressions) {
      if (isIdentifier(expr.property)) {
        this.templateReferencedProperties.add(expr.property.name);
      }
    }
  }

  runUnnecessaryProtectedPropertyCheck() {
    for (const prop of this.protectedProperties) {
      assert.ok(isIdentifier(prop.key));
      const propName = prop.key.name;
      if (this.templateReferencedProperties.has(propName) ||
          this.reflectedProperties.has(propName)) {
        continue;
      }

      this.context.report({
        node: prop,
        messageId: 'unnecessaryProtectedProperty',
        data: {
          propertyName: propName,
          className: this.name,
        },
      });
    }
  }
}

export const litMemberVisibility = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that protected properties on a CrLitElement subclass are referenced in its template',
    },
    messages: {
      unnecessaryProtectedProperty:
          'Property \'{{propertyName}}\' in class \'{{className}}\' is marked \'protected\' but is not referenced in the HTML template. Mark it as \'private\' instead.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const classFilename = context.filename.replaceAll('\\', '/');
    if (classFilename.endsWith('.html.ts')) {
      return {};
    }

    const sourceFiles = getSourceFiles(context);

    const templateFilename = classFilename.replace(/\.ts$/, '.html.ts');
    const templateFile =
        sourceFiles.find(f => f.fileName === templateFilename) || null;

    const classInfos = new Map<string, ClassInfo>();
    let currentClassInfo: ClassInfo|null = null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        if (node.abstract || !isCrLitElementSubclass(node, context) ||
            isMixinClass(node, context)) {
          currentClassInfo = null;
          return;
        }

        assert.ok(node.id);
        currentClassInfo = new ClassInfo(context, node.id.name);
        classInfos.set(node.id.name, currentClassInfo);
      },

      'ClassDeclaration > ClassBody > MethodDefinition[key.name="render"]'(
          node: TSESTree.MethodDefinition) {
        if (currentClassInfo && node.value.body) {
          currentClassInfo.inlinedRenderBlock = node.value.body;
        }
      },

      [`ClassDeclaration > ${PROPERTIES_METHOD_PROPERTY_SELECTOR}`](
          node: TSESTree.Property) {
        if (!currentClassInfo || !isIdentifier(node.key) ||
            !isType(node.value, Node.ObjectExpression)) {
          return;
        }
        const hasReflect = node.value.properties.some(
            p => isType(p, Node.Property) && isIdentifier(p.key) &&
                p.key.name === 'reflect' && isLiteral(p.value) &&
                p.value.value === true);
        if (hasReflect) {
          currentClassInfo.reflectedProperties.add(node.key.name);
        }
      },

      'ClassDeclaration > ClassBody > :matches(PropertyDefinition, AccessorProperty)[accessibility="protected"]'(
          node: TSESTree.PropertyDefinition|TSESTree.AccessorProperty) {
        if (!currentClassInfo || node.override || node.static ||
            !isIdentifier(node.key)) {
          return;
        }
        currentClassInfo.protectedProperties.push(node);
      },

      'ClassDeclaration:exit'() {
        currentClassInfo = null;
      },

      'Program:exit'() {
        let fileTemplateBlocks: TSESTree.BlockStatement[]|null = null;
        for (const classInfo of classInfos.values()) {
          if (classInfo.protectedProperties.length === 0) {
            continue;
          }

          if (templateFile && !fileTemplateBlocks) {
            fileTemplateBlocks =
                extractTemplatesFromFile(templateFile, context, sourceFiles);
          }
          const templateBlocks: TSESTree.BlockStatement[] =
              fileTemplateBlocks ??
              (classInfo.inlinedRenderBlock ? [classInfo.inlinedRenderBlock] :
                                              []);

          for (const block of templateBlocks) {
            classInfo.visitTemplateBlock(block);
          }
          classInfo.runUnnecessaryProtectedPropertyCheck();
        }
      },
    };
  },
});
