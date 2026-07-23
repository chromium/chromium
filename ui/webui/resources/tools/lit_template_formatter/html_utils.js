// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {exec} from 'node:child_process';
import {join} from 'node:path';

// Tags for elements that are only allowed to have a restricted set of
// elements as children, e.g. <select> can only have <option> children,
// mapped to substitutions used before passing the HTML to parse5. This is
// to prevent special Lit placeholder tags from being removed by the parser
// when they occur as "children" of these elements.
export const RESTRICTED_TAGS = {
  'select': 'lit-select',
  'table': 'lit-table',
  'tbody': 'lit-tbody',
  'thead': 'lit-thead',
  'tfoot': 'lit-tfoot',
  'tr': 'lit-tr',
  'td': 'lit-td',
  'th': 'lit-th',
};

export const VOID_ELEMENTS = [
  'area',
  'base',
  'br',
  'col',
  'embed',
  'hr',
  'img',
  'input',
  'link',
  'meta',
  'param',
  'source',
  'track',
  'wbr',
];

export const INDENT_SIZE = 2;
export const LINE_LENGTH_LIMIT = 80;
export const WRAPPED_LINE_INDENT_SIZE = 4;
export const TRAILING_NEWLINE_REGEX = /\n[ \t]*$/;

export const EXPR_PREFIX = 'lit-expr-placeholder';
export const TEMPLATE_PREFIX = 'lit-placeholder';
export const FALSE_TEMPLATE_PREFIX = 'lit-false-placeholder';
export const PROP_PREFIX = 'lit-prop';
export const FORMAT_OFF_PREFIX = 'lit-template-format-off';

/**
 * Returns the formatting depth for a tag name's own indentation. "if" elements
 * are left-aligned, so their indent depth is 0.
 * @param {string} tagName The tag name string.
 * @param {number} currentDepth The normal depth at this point in the tree.
 * @return {number}
 */
export function getDepthForTagName(tagName, currentDepth) {
  return tagName === 'if' ? 0 : currentDepth;
}

/**
 * Returns the formatting depth for a node's own indentation.
 * @param {Object} node The AST node.
 * @param {number} currentDepth The normal depth at this point in the tree.
 * @return {number}
 */
export function getDepthForNode(node, currentDepth) {
  return getDepthForTagName(node.nodeName, currentDepth);
}

/**
 * Returns the next depth to pass when recursing into children of a node.
 * "if" elements are not considered part of the HTML structural tree, so they do
 * not increment depth.
 * @param {Object} node The AST node.
 * @param {number} currentDepth The normal depth at this point in the tree.
 * @return {number}
 */
export function getChildDepthForNode(node, currentDepth) {
  return node.nodeName === 'if' ? currentDepth : currentDepth + 1;
}

/**
 * Returns a newline string followed by the specified number of spaces.
 * @param {number} indent The number of spaces to indent.
 * @return {string}
 */
export function getIndentationPrefix(indent) {
  return '\n' +
      ' '.repeat(indent);
}

/**
 * Executes a shell command asynchronously, optionally writing a string to
 * stdin.
 * @param {string} command The shell command to execute.
 * @param {string} [inputStr] Optional input string to write to stdin.
 * @return {Promise<string>} Resolves with stdout.
 */
export function execAsync(command, inputStr = undefined) {
  const {promise, resolve, reject} = Promise.withResolvers();
  const child = exec(command, (error, stdout) => {
    if (error) {
      reject(error);
    } else {
      resolve(stdout);
    }
  });
  if (inputStr !== undefined) {
    child.stdin.write(inputStr);
    child.stdin.end();
  }
  return promise;
}

/**
 * Returns the path to the depot_tools clang_format.py wrapper script.
 * This wrapper automatically resolves and executes the correct platform binary.
 * @param {string} workspaceRoot Absolute path to the Chromium root directory.
 * @return {string} Path to clang_format.py wrapper script.
 */
export function getClangFormatPath(workspaceRoot) {
  return join(workspaceRoot, 'third_party', 'depot_tools', 'clang_format.py');
}
