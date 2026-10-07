// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESLint, TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import {AST_NODE_TYPES as Node, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type ts from '/third_party/node/node_modules/typescript/lib/typescript.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, isCrLitElementSubclass, isIdentifier, isLiteral, isType} from './query_utils.js';

type Options = [];
type MessageIds = 'incorrectDollarSignNotation'|'missingId'|
    'missingIdNoTemplateFile'|'conditionalId'|'conditionalIdNoTemplateFile';

function extractDomIdsFromTemplateLiteral(node: TSESTree.TemplateLiteral):
    string[] {
  // Regular expression to extract all DOM ids from a string.
  const DOM_ID_REGEX = /id\s*=\s*"(?<domId>[A-Za-z0-9\-]+)"/g;
  const ids: string[] = [];
  for (const quasi of node.quasis) {
    const matches = Array.from(quasi.value.raw.matchAll(DOM_ID_REGEX));
    for (const match of matches) {
      assert.ok(match.groups);
      ids.push(match.groups['domId']!);
    }
  }
  return ids;
}

function collectTemplateLiterals(
    node: TSESTree.Node,
    results: TSESTree.TemplateLiteral[] = []): TSESTree.TemplateLiteral[] {
  if (node.type === Node.TemplateLiteral) {
    results.push(node);
  }
  for (const key of Object.keys(node)) {
    if (key === 'parent') {
      continue;
    }
    const child = (node as unknown as Record<string, unknown>)[key];
    if (Array.isArray(child)) {
      for (const item of child) {
        if (item && typeof item === 'object' && 'type' in item) {
          collectTemplateLiterals(item as TSESTree.Node, results);
        }
      }
    } else if (child && typeof child === 'object' && 'type' in child) {
      collectTemplateLiterals(child as TSESTree.Node, results);
    }
  }
  return results;
}

function extractTemplateFromFile(
    context: TSESLint.RuleContext<MessageIds, Options>,
    templateFile: ts.SourceFile): TSESTree.BlockStatement|null {
  const parser = context.languageOptions.parser as TSESLint.Parser.ParserModule;
  const parserOptions = context.languageOptions.parserOptions;
  assert.ok(parser && 'parse' in parser);
  const templateAst = parser.parse(templateFile.text, {
    ...parserOptions,
    filePath: templateFile.fileName,
  }) as TSESTree.Program;
  for (const statement of templateAst.body) {
    const decl = statement.type === Node.ExportNamedDeclaration ?
        statement.declaration! :
        statement;
    if (decl.type === Node.FunctionDeclaration && decl.id!.name === 'getHtml') {
      return decl.body;
    }
  }
  return null;
}

// Necessary info to track about each CrLitElement subclass definition
// encountered in the current file.
class ClassInfo {
  private readonly context: TSESLint.RuleContext<MessageIds, Options>;
  private name: string;
  private templateFile: ts.SourceFile|null;
  inlinedRenderBlock: TSESTree.BlockStatement|null = null;
  private interfaceMembers: TSESTree.TSPropertySignature[] = [];
  private topLevelDomIds: string[] = [];
  private nestedDomIds: string[] = [];

  constructor(
      context: TSESLint.RuleContext<MessageIds, Options>, name: string,
      dollarProperty: TSESTree.TSPropertySignature,
      templateFile: ts.SourceFile|null) {
    this.context = context;
    this.name = name;
    this.templateFile = templateFile;

    assert.ok(dollarProperty.typeAnnotation);
    const typeAnnotation = dollarProperty.typeAnnotation.typeAnnotation;
    assert.ok(isType(typeAnnotation, Node.TSTypeLiteral));
    for (const member of typeAnnotation.members) {
      assert.ok(isType(member, Node.TSPropertySignature));
      this.interfaceMembers.push(member);
    }
  }

  visitTemplateBlock(templateBlock: TSESTree.BlockStatement) {
    const returnStatement = templateBlock.body.find(
        (statement): statement is TSESTree.ReturnStatement =>
            statement.type === Node.ReturnStatement);
    assert.ok(returnStatement && returnStatement.argument);
    assert.ok(
        returnStatement.argument.type === Node.TaggedTemplateExpression &&
        returnStatement.argument.quasi.type === Node.TemplateLiteral);
    const topLevelTemplateLiteral = returnStatement.argument.quasi;

    const allTemplateLiterals = collectTemplateLiterals(templateBlock);
    for (const node of allTemplateLiterals) {
      const ids = extractDomIdsFromTemplateLiteral(node);
      if (node === topLevelTemplateLiteral) {
        this.topLevelDomIds.push(...ids);
      } else {
        this.nestedDomIds.push(...ids);
      }
    }
  }

  runIncorrectDollarSignNotationCheck() {
    for (const member of this.interfaceMembers) {
      if (!isLiteral(member.key)) {
        continue;
      }

      this.context.report({
        node: member,
        messageId: 'incorrectDollarSignNotation',
        data: {
          className: this.name,
          incorrectIdentifier: member.key.value as string,
          suggestedName: dashCaseToCamelCase(member.key.value as string),
        },
      });
    }
  }

  runMissingIdCheck() {
    for (const member of this.interfaceMembers) {
      if (!isIdentifier(member.key)) {
        continue;
      }

      const domId = member.key.name;
      if (this.topLevelDomIds.includes(domId) ||
          this.nestedDomIds.includes(domId)) {
        continue;
      }

      if (this.templateFile) {
        this.context.report({
          node: member,
          messageId: 'missingId',
          data: {
            domId,
            className: this.name,
            templateFile: path.basename(this.templateFile.fileName),
          },
        });
        continue;
      }

      this.context.report({
        node: member,
        messageId: 'missingIdNoTemplateFile',
        data: {
          domId,
          className: this.name,
        },
      });
    }
  }

  runConditionalIdCheck() {
    for (const member of this.interfaceMembers) {
      if (!isIdentifier(member.key)) {
        continue;
      }

      const domId = member.key.name;
      if (!this.nestedDomIds.includes(domId)) {
        continue;
      }

      if (this.templateFile) {
        this.context.report({
          node: member,
          messageId: 'conditionalId',
          data: {
            domId,
            className: this.name,
            templateFile: path.basename(this.templateFile.fileName),
          },
        });
        continue;
      }

      this.context.report({
        node: member,
        messageId: 'conditionalIdNoTemplateFile',
        data: {
          domId,
          className: this.name,
        },
      });
    }
  }
}

export const litElementInvalidInterface = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that all ids referenced in a Lit element\'s interface actually exist unconditionally in its template',
    },
    messages: {
      incorrectDollarSignNotation:
          'Use camelCase identifiers, not dash-case literals, for DOM ids in the interface. Change \'{{incorrectIdentifier}}\' to {{suggestedName}} in the interface for {{className}}.',
      missingId:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but no element with that ID was found in the template file \'{{templateFile}}\'.',
      missingIdNoTemplateFile:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but no element with that ID was found in the template.',
      conditionalId:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but the element is conditionally rendered in the template file \'{{templateFile}}\'.',
      conditionalIdNoTemplateFile:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but the element is conditionally rendered in the template.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    const classFilename = context.filename.replaceAll('\\', '/');
    const templateFilename = classFilename.replace(/\.ts$/, '.html.ts');
    const templateFile =
        sourceFiles.find(f => f.fileName === templateFilename) || null;

    // Dollar sign property signatures in this file, keyed by interface name.
    const dollarProperties = new Map<string, TSESTree.TSPropertySignature>();

    // Info about all the CrLitElement subclass definitions encountered in this
    // file.
    const classInfos = new Map<string, ClassInfo>();
    let currentClassInfo: ClassInfo|null = null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        if (!isCrLitElementSubclass(node, context.sourceCode.ast)) {
          currentClassInfo = null;
          return;
        }

        assert.ok(node.id);
        const dollarProperty = dollarProperties.get(node.id.name) || null;
        if (!dollarProperty) {
          currentClassInfo = null;
          return;
        }

        currentClassInfo =
            new ClassInfo(context, node.id.name, dollarProperty, templateFile);
        classInfos.set(node.id.name, currentClassInfo);
      },

      'ClassDeclaration > ClassBody > MethodDefinition[key.name="render"]'(
          node: TSESTree.MethodDefinition) {
        if (currentClassInfo && node.value.body) {
          currentClassInfo.inlinedRenderBlock = node.value.body;
        }
      },

      'TSInterfaceDeclaration > TSInterfaceBody > TSPropertySignature[key.name="$"]'(
          node: TSESTree.TSPropertySignature) {
        assert.ok(node.parent && node.parent.parent);
        assert.ok(isType(node.parent.parent, Node.TSInterfaceDeclaration));
        dollarProperties.set(node.parent.parent.id.name, node);
      },

      'Program:exit'() {
        for (const classInfo of classInfos.values()) {
          const templateBlock: TSESTree.BlockStatement|null = templateFile ?
              extractTemplateFromFile(context, templateFile) :
              classInfo.inlinedRenderBlock;
          assert.ok(templateBlock);
          classInfo.visitTemplateBlock(templateBlock);

          classInfo.runIncorrectDollarSignNotationCheck();
          classInfo.runMissingIdCheck();
          classInfo.runConditionalIdCheck();
        }
      },
    };
  },
});
