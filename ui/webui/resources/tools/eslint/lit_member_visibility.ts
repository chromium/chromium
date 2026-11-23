// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ParserServices, TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';

import {extractTemplatesFromFile, getSourceFiles, isCrLitElementSubclass, isIdentifier, isLiteral, isMixinClass, isType, matchesTypeInHierarchy, PROPERTIES_METHOD_PROPERTY_SELECTOR, THIS_MEMBER_EXPRESSION_QUERY} from './query_utils.js';

type Options = [];
type MessageIds = 'unnecessaryProtectedMethod'|'unnecessaryProtectedProperty';

function matchesBaseType(
    classNode: ts.ClassLikeDeclaration, checker: ts.TypeChecker,
    predicate: (t: ts.Type) => boolean): boolean {
  const symbol = checker.getTypeAtLocation(classNode).symbol;
  if (!symbol) {
    return false;
  }
  const classType = checker.getDeclaredTypeOfSymbol(symbol);
  if ((classType.flags & ts.TypeFlags.Object) === 0 ||
      ((classType as ts.ObjectType).objectFlags &
       ts.ObjectFlags.ClassOrInterface) === 0) {
    return false;
  }
  return checker.getBaseTypes(classType as ts.InterfaceType)
      .some(base => matchesTypeInHierarchy(base, checker, predicate));
}

// Collects symbols of classes extended anywhere in `sourceFiles`. Note that
// `getSourceFiles()` only covers files in the current TS build target's
// `rootDir`, so subclasses in downstream targets (e.g. a shared
// `ui/webui/resources` element extended in `chrome/browser/resources`) are not
// detected here.
function getExtendedClassSymbols(
    sourceFiles: ts.SourceFile[], checker: ts.TypeChecker): Set<ts.Symbol> {
  const symbols = new Set<ts.Symbol>();
  function visit(node: ts.Node): void {
    if ((ts.isClassDeclaration(node) || ts.isClassExpression(node)) &&
        node.heritageClauses?.some(
            c => c.token === ts.SyntaxKind.ExtendsKeyword)) {
      matchesBaseType(node, checker, t => {
        if (t.symbol) {
          symbols.add(t.symbol);
        }
        return false;
      });
    }
    ts.forEachChild(node, visit);
  }
  for (const sf of sourceFiles) {
    visit(sf);
  }
  return symbols;
}

function isClassExtended(
    classNode: TSESTree.ClassDeclaration, services: ParserServices,
    checker: ts.TypeChecker, extendedClassSymbols: Set<ts.Symbol>): boolean {
  const tsClassNode = services.esTreeNodeToTSNodeMap.get(classNode);
  assert.ok(tsClassNode);
  const classSymbol = checker.getTypeAtLocation(tsClassNode).symbol;
  return Boolean(classSymbol && extendedClassSymbols.has(classSymbol));
}

// TypeScript's `noImplicitOverride` does not require `override` when a concrete
// subclass implements an `abstract` base class member, and such members cannot
// be marked `private`.
function isDeclaredInBaseClass(
    classNode: TSESTree.ClassDeclaration, memberName: string,
    services: ParserServices, checker: ts.TypeChecker): boolean {
  const tsClassNode = services.esTreeNodeToTSNodeMap.get(classNode);
  assert.ok(tsClassNode && ts.isClassLike(tsClassNode));
  return matchesBaseType(
      tsClassNode, checker, t => Boolean(t.getProperty(memberName)));
}

function classifyMethodType(kind: TSESTree.MethodDefinition['kind']): 'Getter'|
    'Setter'|'Method' {
  switch (kind) {
    case 'get':
      return 'Getter';
    case 'set':
      return 'Setter';
    default:
      return 'Method';
  }
}

class ClassInfo {
  private readonly context: TSESLint.RuleContext<MessageIds, Options>;
  readonly node: TSESTree.ClassDeclaration;
  private readonly name: string;
  readonly protectedProperties:
      Array<TSESTree.PropertyDefinition|TSESTree.AccessorProperty> = [];
  readonly protectedMethods: TSESTree.MethodDefinition[] = [];
  readonly reflectedProperties: Set<string> = new Set();
  inlinedRenderBlock: TSESTree.BlockStatement|null = null;
  private readonly templateReferencedMembers: Set<string> = new Set();

  constructor(
      context: TSESLint.RuleContext<MessageIds, Options>,
      node: TSESTree.ClassDeclaration, name: string) {
    this.context = context;
    this.node = node;
    this.name = name;
  }

  visitTemplateBlock(templateBlock: TSESTree.BlockStatement): void {
    const thisExpressions =
        esquery.match(templateBlock, THIS_MEMBER_EXPRESSION_QUERY) as
        TSESTree.MemberExpression[];
    for (const expr of thisExpressions) {
      if (isIdentifier(expr.property)) {
        this.templateReferencedMembers.add(expr.property.name);
      }
    }
  }

  getUnreferencedProtectedProperties(
      services: ParserServices, checker: ts.TypeChecker):
      Array<TSESTree.PropertyDefinition|TSESTree.AccessorProperty> {
    return this.protectedProperties.filter(prop => {
      assert.ok(isIdentifier(prop.key));
      const propName = prop.key.name;
      return !this.templateReferencedMembers.has(propName) &&
          !this.reflectedProperties.has(propName) &&
          !isDeclaredInBaseClass(this.node, propName, services, checker);
    });
  }

  getUnreferencedProtectedMethods(
      services: ParserServices,
      checker: ts.TypeChecker): TSESTree.MethodDefinition[] {
    return this.protectedMethods.filter(method => {
      assert.ok(isIdentifier(method.key));
      const methodName = method.key.name;
      return !this.templateReferencedMembers.has(methodName) &&
          !isDeclaredInBaseClass(this.node, methodName, services, checker);
    });
  }

  reportViolations(
      unreferencedProps:
          Array<TSESTree.PropertyDefinition|TSESTree.AccessorProperty>,
      unreferencedMethods: TSESTree.MethodDefinition[]): void {
    for (const prop of unreferencedProps) {
      assert.ok(isIdentifier(prop.key));
      this.context.report({
        node: prop,
        messageId: 'unnecessaryProtectedProperty',
        data: {
          propertyName: prop.key.name,
          className: this.name,
        },
      });
    }
    for (const method of unreferencedMethods) {
      assert.ok(isIdentifier(method.key));
      this.context.report({
        node: method,
        messageId: 'unnecessaryProtectedMethod',
        data: {
          methodType: classifyMethodType(method.kind),
          methodName: method.key.name,
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
          'Ensures that protected properties and methods on a CrLitElement subclass are referenced in its template',
    },
    messages: {
      unnecessaryProtectedMethod:
          '{{methodType}} \'{{methodName}}\' in class \'{{className}}\' is marked \'protected\' but is not referenced in the HTML template. Mark it as \'private\' instead.',
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

    const services = ESLintUtils.getParserServices(context);
    const checker = services.program.getTypeChecker();
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
        currentClassInfo = new ClassInfo(context, node, node.id.name);
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

      'ClassDeclaration > ClassBody > MethodDefinition[accessibility="protected"][kind!="constructor"]'(
          node: TSESTree.MethodDefinition) {
        if (!currentClassInfo || node.override || node.static ||
            !isIdentifier(node.key)) {
          return;
        }
        currentClassInfo.protectedMethods.push(node);
      },

      'ClassDeclaration:exit'() {
        currentClassInfo = null;
      },

      'Program:exit'() {
        let fileTemplateBlocks: TSESTree.BlockStatement[]|null = null;
        let extendedClassSymbols: Set<ts.Symbol>|null = null;
        for (const classInfo of classInfos.values()) {
          if (classInfo.protectedProperties.length === 0 &&
              classInfo.protectedMethods.length === 0) {
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

          const unreferencedProps =
              classInfo.getUnreferencedProtectedProperties(services, checker);
          const unreferencedMethods =
              classInfo.getUnreferencedProtectedMethods(services, checker);
          if (unreferencedProps.length === 0 &&
              unreferencedMethods.length === 0) {
            continue;
          }

          // Skip concrete CrLitElement subclasses that are extended by another
          // class in this target, as subclasses may access their `protected`
          // members in TypeScript or in their own HTML templates.
          if (!extendedClassSymbols) {
            extendedClassSymbols =
                getExtendedClassSymbols(sourceFiles, checker);
          }
          if (isClassExtended(
                  classInfo.node, services, checker, extendedClassSymbols)) {
            continue;
          }

          classInfo.reportViolations(unreferencedProps, unreferencedMethods);
        }
      },
    };
  },
});
