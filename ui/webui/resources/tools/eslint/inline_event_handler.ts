// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESLintUtils} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import type {TSESTree} from '/third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
import assert from 'node:assert';

import {isIdentifier} from './query_utils.js';

type Options = [];
type MessageIds = 'inlineEventHandlerFound';

export const inlineEventHandler = ESLintUtils.RuleCreator.withoutDocs<
    Options, MessageIds>({
  meta: {
    type: 'problem',
    docs: {
      description:
          'Ensures that event handlers are not inlined in Lit/Polymer HTML templates',
    },
    messages: {
      inlineEventHandlerFound:
          'Inline event handler for event \'{{eventName}}\' found on element \'{{tagName}}\'. Do not use inline arrow functions in templates',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    const templateFilename = context.filename.replaceAll('\\', '/');
    assert.ok(templateFilename.endsWith('.html.ts'));

    // Regular expression to extract all inline lambda event handlers from a
    // string.
    const EVENT_HANDLER_REGEX =
        /@(?<eventName>[a-zA-Z0-9-]+)\s*=\s*"\$\{\s*\(?.*?\)?\s*=>[\s\S]*?\}"/g;

    return {
      ['FunctionDeclaration[id.name=/getHtml|getTemplate/]'](
          node: TSESTree.FunctionDeclarationWithName) {
        // Looking for either of the following patterns
        //  - Lit templates: 'getHtml(this: SomeType) {...}'
        //  - Polymer templates: 'getTemplate() {...}'

        if (node.id.name === 'getHtml' &&
            (node.params.length !== 1 || !isIdentifier(node.params[0]!) ||
             node.params[0]!.name !== 'this')) {
          // Handle a few cases where lit-html is used directly and there is no
          // classDefinitionFilename file.
          return;
        }

        // Extract function's body as a string.
        const bodyString = context.sourceCode.getText(node.body);
        const matches = Array.from(bodyString.matchAll(EVENT_HANDLER_REGEX));
        if (matches.length === 0) {
          return;
        }

        const eventNames = matches.map(match => {
          assert.ok(match.groups);
          return match.groups['eventName'];
        });
        const tagNames = matches.map(match => {
          const tagNameStart =
              bodyString.substring(0, match.index).lastIndexOf('<') + 1;
          const tagNameLength = bodyString.substring(tagNameStart).indexOf(' ');
          return bodyString.substring(
              tagNameStart, tagNameStart + tagNameLength);
        });

        for (let i = 0; i < eventNames.length; i++) {
          context.report({
            node,
            messageId: 'inlineEventHandlerFound',
            data: {
              eventName: eventNames[i],
              tagName: tagNames[i],
            },
          });
        }
      },
    };
  },
});
