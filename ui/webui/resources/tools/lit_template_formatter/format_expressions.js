// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {FALSE_TEMPLATE_PREFIX, FORMAT_OFF_PREFIX, PROP_PREFIX, runClangFormat, WRAPPED_LINE_INDENT_SIZE} from './html_utils.js';

const ExpressionType = {
  EXPRESSION: 'expression',
  TERNARY: 'ternary',
  ARROW: 'arrow',
  TERNARY_FALSE: 'ternary_false',
};

/**
 * Defines formatting for a type of Lit expression.
 * @typedef {Object} ExpressionFormatter
 * @property {function(string): string} unwrap Returns the TS code inside the
 *     expression, i.e. strips opening "${" and/or whitespace.
 * @property {function(string): string} wrap Adds minimal code necessary to
 *     make the expression valid TypeScript.
 * @property {function(string): string} restore Transforms the formatted TS
 *     code as needed to put it back in the template. Combined with
 *     prependToFinal, reverses any transformations performed in unwrap/wrap.
 * @property {string} prependToFinal String that should be prepended to the
 *     restored expression when it is replaced in the template.
 * @property {number} columnLimitAdjustment How much the column limit should be
 *     reduced when running clang-format on the expression's TS contents to
 *     prevent the final template contents from exceeding 80 characters.
 */

/** @type {Object<ExpressionType, ExpressionFormatter>} */
const ExpressionConfig = {
  [ExpressionType.EXPRESSION]: {
    unwrap: (code) => code.substring(2, code.length - 1),
    wrap: (code) => code,
    restore: (formatted) => formatted + '}',
    prependToFinal: '${',
    columnLimitAdjustment: -3,
  },
  [ExpressionType.TERNARY]: {
    unwrap: (code) => code.substring(2),
    wrap: (code) => code + '` : \'\'',
    restore: (formatted) => formatted.replace(/\s*`\s*:\s*['"]['"]\s*$/, ''),
    prependToFinal: '${',
    columnLimitAdjustment: -2,
  },
  [ExpressionType.ARROW]: {
    unwrap: (code) => code.substring(2),
    wrap: (code) => code + '`)',
    restore: (formatted) => formatted.replace(/\s*`\s*\)\s*$/, ''),
    prependToFinal: '${',
    columnLimitAdjustment: -2,
  },
  [ExpressionType.TERNARY_FALSE]: {
    unwrap: (code) => code,
    wrap: (code) => 'true ? null' + code + '`',
    restore: (formatted) => {
      let res = formatted.replace(/^\s*true\s*\?\s*null/, '');
      res = res.replace(/\s*`\s*$/, '');
      return res;
    },
    prependToFinal: '',
    columnLimitAdjustment: 12,
  },
};

/**
 * Computes the column limit for clang-format for an expression.
 * For attributes (e.g. attr="${expr}"), the initial limit accounts for the
 * attribute indentation (WRAPPED_LINE_INDENT_SIZE) but not the attribute name
 * itself, since subsequent lines of multiline expressions are aligned with the
 * attribute indent rather than the attribute name. If the first line of the
 * formatted expression exceeds the space available with the attribute name, it
 * will be wrapped to a new line after "${".
 */
function computeColumnLimit(value, type) {
  const indent = value.indent || 0;
  const attrName = value.attrName;
  const config = ExpressionConfig[type];
  const baseIndent = indent + (attrName ? WRAPPED_LINE_INDENT_SIZE : 0);
  return 80 - baseIndent + config.columnLimitAdjustment;
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
    if (key.startsWith('/') || key.startsWith(PROP_PREFIX) ||
        key.startsWith(FORMAT_OFF_PREFIX) || value.nested ||
        (key.startsWith(FALSE_TEMPLATE_PREFIX) && !value.isTemplate)) {
      // Skip closing tags, property name placeholders,
      // format-off placeholders, nested templates, and false template
      // placeholders that are not actual templates.
      continue;
    }

    const code = value.code;
    let type = ExpressionType.EXPRESSION;

    if (key.startsWith(FALSE_TEMPLATE_PREFIX)) {
      type = ExpressionType.TERNARY_FALSE;
    } else if (code.startsWith('${') && code.endsWith('}')) {
      type = ExpressionType.EXPRESSION;
    } else if (code.startsWith('${')) {
      if (/\?\s*html`$/.test(code)) {
        type = ExpressionType.TERNARY;
      } else {
        assert.ok(/=>\s*html`$/.test(code));
        type = ExpressionType.ARROW;
      }
    }

    const config = ExpressionConfig[type];
    let codeToFormat = config.wrap(config.unwrap(code));
    codeToFormat =
        codeToFormat.trim().split('\n').map(l => l.trim()).join('\n');

    const limit = computeColumnLimit(value, type);

    // If the snippet is a simple identifier, property access, or negation that
    // already fits within the column limit, clang-format will never modify it.
    // Skip spawning a process to improve formatting speed.
    const isSimpleToken = /^!*[a-zA-Z0-9_$.]+$/.test(codeToFormat) &&
        codeToFormat.length <= limit;
    let formattedCode = codeToFormat;
    if (!isSimpleToken) {
      // Run clang-format on the snippet using inline JSON style override
      const style = `{BasedOnStyle: Chromium, ColumnLimit: ${limit}}`;
      formattedCode = await runClangFormat(
          clangFormatPath, ['-assume-filename=f.ts', `-style=${style}`],
          codeToFormat);

      // Remove trailing newline added by clang-format if any
      formattedCode = formattedCode.replace(/\n$/, '');
    }

    let baseIndent =
        (value.indent || 0) + (value.attrName ? WRAPPED_LINE_INDENT_SIZE : 0);
    // For attributes (e.g. attr="${expr}"), the opening line has the attribute
    // name, '="' (2 chars), the closing '"' (1 char), and potentially the
    // closing '>' (1 char) on the tag. Subtract the additional characters
    // beyond what columnLimitAdjustment (-3 for '${' and '}') already accounts
    // for to check if the first line fits with the attribute name. If not, put
    // the expression on a new line after "${". Note: this will be slightly
    // conservative for cases where ">" does not occur on the first line, but
    // line breaks have not been determined at this point.
    const firstLineLimit =
        value.attrName ? limit - (value.attrName.length + 4) : limit;
    if (value.attrName &&
        (formattedCode.includes('\n') ||
         formattedCode.length > firstLineLimit)) {
      baseIndent += WRAPPED_LINE_INDENT_SIZE;
      const newLimit = 80 - baseIndent + config.columnLimitAdjustment;
      if (newLimit !== limit) {
        const newStyle = `{BasedOnStyle: Chromium, ColumnLimit: ${newLimit}}`;
        formattedCode = await runClangFormat(
            clangFormatPath, ['-assume-filename=f.ts', `-style=${newStyle}`],
            codeToFormat);
        formattedCode = formattedCode.replace(/\n$/, '');
      }
      formattedCode = '\n' + formattedCode;
    }

    // Apply indentation to later lines of multiline expressions.
    if (formattedCode.includes('\n')) {
      const exprLines = formattedCode.split('\n');
      const indentStr = ' '.repeat(baseIndent);
      formattedCode = exprLines[0] + '\n' +
          exprLines.slice(1).map(l => `${indentStr}${l}`).join('\n');
    }

    const restored = config.restore(formattedCode);
    placeholderMap.set(
        key, {...value, code: `${config.prependToFinal}${restored}`});
  }
}
