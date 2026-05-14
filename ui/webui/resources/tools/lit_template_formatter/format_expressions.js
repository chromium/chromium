// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {execAsync, FALSE_TEMPLATE_PREFIX, FORMAT_OFF_PREFIX, PROP_PREFIX} from './html_utils.js';

const ExpressionType = {
  EXPRESSION: 'expression',
  TERNARY: 'ternary',
  ARROW: 'arrow',
};

const suffixMap = new Map([
  [ExpressionType.EXPRESSION, ''],
  [ExpressionType.TERNARY, '` : \'\''],
  [ExpressionType.ARROW, '`)'],
]);

function computeColumnLimit(value, type) {
  const indent = value.indent || 0;
  const attrName = value.attrName;
  // For expressions, subtract 1 because we need to restore the closing "}".
  // For other types, add space for the suffix, since this is stripped after
  // formatting.
  const adjustment =
      type === ExpressionType.EXPRESSION ? -1 : suffixMap.get(type).length;
  // 80 chars, subtract the indent, subtract 2 for the opening "${".
  let limit = 80 - indent - 2 + adjustment;
  if (attrName) {
    // Maximum width is if we wrap to a new line, indented by 4 from the
    // parent tag.
    limit = limit - attrName.length - 4;
  }
  return limit;
}

/**
 * Formats TS expressions in the map using clang-format.
 * @param {Map<string, {code: string, indent?: number, attrName?: string,
 *     nested?: boolean}>} placeholderMap The map to update.
 * @param {string} clangFormatPath Path to clang-format binary.
 * @param {string} filePath Path to the file being formatted.
 */
export async function formatTsExpressions(
    placeholderMap, clangFormatPath, _filePath) {
  for (const [key, value] of placeholderMap.entries()) {
    if (key.startsWith('/') || key.startsWith(FALSE_TEMPLATE_PREFIX) ||
        key.startsWith(PROP_PREFIX) || key.startsWith(FORMAT_OFF_PREFIX) ||
        value.nested) {
      // Skip closing tags, false tags, property name placeholders,
      // format-off placeholders, and nested templates.
      continue;
    }

    let code = value.code;
    let type = ExpressionType.EXPRESSION;

    if (code.startsWith('${') && code.endsWith('}')) {
      type = ExpressionType.EXPRESSION;
    } else if (code.startsWith('${')) {
      if (code.endsWith('? html`')) {
        type = ExpressionType.TERNARY;
      } else {
        assert.ok(code.endsWith('=> html`'));
        type = ExpressionType.ARROW;
      }
    }

    // Compute code to format. Add suffix where needed to make valid TS.
    code = type === ExpressionType.EXPRESSION ?
        code.substring(2, code.length - 1) :
        code.substring(2) + suffixMap.get(type);

    const limit = computeColumnLimit(value, type);

    // Run clang-format on the snippet using inline JSON style override
    const style = `{BasedOnStyle: Chromium, ColumnLimit: ${limit}}`;
    let formattedCode = await execAsync(
        `"${clangFormatPath}" -assume-filename=dummy.ts -style="${style}"`,
        code);

    // Remove trailing newline added by clang-format if any
    formattedCode = formattedCode.replace(/\n$/, '');

    // Apply indentation to later lines of multiline expressions.
    if (formattedCode.includes('\n')) {
      const exprLines = formattedCode.split('\n');
      const indentStr = ' '.repeat(value.indent || 0);
      formattedCode = exprLines[0] + '\n' +
          exprLines.slice(1).map(l => `${indentStr}${l}`).join('\n');
    }

    // Remove any added suffix, restore the closing "}" if needed, and
    // put the formatted expression into the placeholder map.
    formattedCode = type === ExpressionType.EXPRESSION ?
        formattedCode + '}' :
        formattedCode.substring(
            0, formattedCode.length - suffixMap.get(type).length);
    placeholderMap.set(key, {...value, code: `\${${formattedCode}`});
  }
}
