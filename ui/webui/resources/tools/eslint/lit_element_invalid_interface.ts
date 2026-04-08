// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AST_NODE_TYPES, ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import esquery from '/third_party/node/node_modules/esquery/dist/esquery.esm.min.js';
import assert from 'node:assert';
import path from 'node:path';

import {dashCaseToCamelCase, isCrLitElementSubclass, isIdentifier, isLiteral} from './query_utils.js';

type Options = [];
type MessageIds =
    'incorrectDollarSignNotation'|'missingId'|'missingIdNoTemplateFile';

export const litElementInvalidInterface = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that all ids referenced in a Lit element\'s interface actually exist in its template',
    },
    messages: {
      incorrectDollarSignNotation:
          'Use camelCase identifiers, not dash-case literals, for DOM ids in the interface. Change \'{{incorrectIdentifier}}\' to {{suggestedName}} in the interface for {{className}}.',
      missingId:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but no element with that ID was found in the template file \'{{templateFile}}\'.',
      missingIdNoTemplateFile:
          'Id \'{{domId}}\' is listed in the interface definition for {{className}}, but no element with that ID was found in the template.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    function extractDomIdsFromTemplate(text: string): string[] {
      // Regular expression to extract all DOM ids from a string.
      const DOM_ID_REGEX = /id\s*=\s*"(?<domId>[A-Za-z0-9\-]+)"/g;
      const matches = Array.from(text.matchAll(DOM_ID_REGEX));
      return matches.map(match => {
        assert.ok(match.groups);
        return match.groups['domId']!;
      });
    }

    const services = ESLintUtils.getParserServices(context);
    const compilerOptions = services.program.getCompilerOptions();

    const sourceFiles = services.program.getSourceFiles().filter(
        f => f.fileName.startsWith(compilerOptions.rootDir + '/'));

    const classFilename = context.filename.replaceAll('\\', '/');
    const templateFilename = classFilename.replace(/\.ts$/, '.html.ts');
    const templateFile =
        sourceFiles.find(f => f.fileName === templateFilename) || null;

    return {
      'ClassDeclaration'(node: TSESTree.ClassDeclaration) {
        if (!isCrLitElementSubclass(node, context.sourceCode.ast)) {
          return;
        }
        assert.ok(node.id);
        const className = node.id.name;

        const interfaceSelectorString = `TSInterfaceDeclaration[id.name="${
            className}"] > TSInterfaceBody > TSPropertySignature[key.name="$"]`;
        const matchingNodes = esquery.match(
                                  context.sourceCode.ast,
                                  esquery.parse(interfaceSelectorString)) as
            TSESTree.TSPropertySignature[];
        if (matchingNodes.length === 0) {
          return;
        }
        const interfaceNode = matchingNodes[0]!;

        let domIds: string[] = [];
        if (templateFile) {
          domIds = extractDomIdsFromTemplate(templateFile.text);
        } else {
          // Case where the template is inlined in the render() method.
          const renderNodeSelector = esquery.parse(
              'ClassDeclaration > ClassBody > MethodDefinition[key.name="render"]');
          const matchingNodes = esquery.match(node, renderNodeSelector) as
              TSESTree.MethodDefinition[];
          assert.ok(matchingNodes.length === 1);
          domIds = extractDomIdsFromTemplate(
              context.sourceCode.getText(matchingNodes[0]));
        }

        assert.ok(interfaceNode.typeAnnotation);
        const typeAnnotation = interfaceNode.typeAnnotation.typeAnnotation;
        assert.ok(typeAnnotation.type === AST_NODE_TYPES.TSTypeLiteral);
        const interfaceMembers = typeAnnotation.members;

        for (const member of interfaceMembers) {
          assert.ok(member.type === AST_NODE_TYPES.TSPropertySignature);
          if (isLiteral(member.key)) {
            context.report({
              node: member,
              messageId: 'incorrectDollarSignNotation',
              data: {
                className: className,
                incorrectIdentifier: member.key.value as string,
                suggestedName: dashCaseToCamelCase(member.key.value as string),
              },
            });
            continue;
          }
          assert.ok(isIdentifier(member.key));
          if (domIds.includes(member.key.name)) {
            continue;
          }
          if (templateFile) {
            context.report({
              node: member,
              messageId: 'missingId',
              data: {
                domId: member.key.name,
                className: className,
                templateFile: path.basename(templateFilename),
              },
            });
            continue;
          }
          context.report({
            node: member,
            messageId: 'missingIdNoTemplateFile',
            data: {
              domId: member.key.name,
              className: className,
            },
          });
        }
      },
    };
  },
});
