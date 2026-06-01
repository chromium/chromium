// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';
// Disable no-restricted-syntax to allow NodeJS imports which are extensionless.
// eslint-disable-next-line no-restricted-syntax
import {readFile, unlink, writeFile} from 'node:fs/promises';
import {dirname, join} from 'node:path';
import {argv, exit, stdout} from 'node:process';
import {fileURLToPath} from 'node:url';
import {parseArgs} from 'node:util';

import {formatTsExpressions} from './format_expressions.js';
import {prepareHtmlAst} from './format_html.js';
import {execAsync, getClangFormatPath, WRAPPED_LINE_INDENT_SIZE} from './html_utils.js';
import {processTemplate} from './process_lit_template_ts.js';
import {serializeHtmlAst, serializeNode} from './serialize_html.js';

// Find clang-format path relative to this script
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const workspaceRoot = join(__dirname, '../../../../../');
const clangFormatPath = getClangFormatPath(workspaceRoot);
const diffHelperPath = join(__dirname, 'diff_helper.py');

// Returns the formatted file contents, or null if a Lit HTML template cannot
// be extracted from |filePath|.
async function formatFile(filePath, sortAttributes, quiet = false) {
  const tempFilePath = `${filePath}.tmp.ts`;

  // Step 1: Extract and process template content
  const {
    result: replacedTemplate,
    map: placeholderMap,
    placeholder,
    restOfFile,
  } = processTemplate(filePath);

  if (!replacedTemplate) {
    if (!quiet) {
      console.info(`Could not extract template from ${filePath}, skipping`);
    }
    return null;
  }

  // Step 2: Prepare HTML AST and collect metadata
  const ast = prepareHtmlAst(replacedTemplate, placeholderMap);

  // Step 2b: Preprocess nested templates within tags. Prepare their ASTs to
  // collect metadata for their nested expressions.
  const nestedTemplates = [];
  placeholderMap.forEach((content, placeholder) => {
    if (!content.nested) {
      return;
    }
    // Nested templates have the indent of their containing tag + 4 (since
    // the template will behave like a wrapped attribute).
    // depth = indent / 2
    const depth = (content.indent + WRAPPED_LINE_INDENT_SIZE) / 2;
    const internalAst = prepareHtmlAst(content.code, placeholderMap, depth);
    nestedTemplates.push({placeholder, internalAst, depth});
  });

  // Step 3: Format TS expressions using collected metadata
  await formatTsExpressions(placeholderMap, clangFormatPath, filePath);

  // Step 4a: Serialize nested templates now that their expressions are
  // formatted.
  nestedTemplates.forEach(({placeholder, internalAst, depth}) => {
    let formatted =
        serializeNode(internalAst, depth, placeholderMap, sortAttributes);
    // The nested template should go on the same line as the attribute name
    // it is attached to with no spacing, so cut off all the leading
    // whitespace added by the formatter that assumes it's a normal template.
    formatted = formatted.trimStart();
    placeholderMap.set(placeholder, {code: formatted, nested: true});
  });

  // Step 4b: Serialize HTML AST back to string
  const formattedHtml = serializeHtmlAst(ast, placeholderMap, sortAttributes);

  // Step 5: Write rest of file to temp file, run clang-format.
  await writeFile(tempFilePath, restOfFile, 'utf-8');
  if (!quiet) {
    console.info(`Running clang-format on temporary file...`);
  }
  await execAsync(`python3 "${clangFormatPath}" -i "${tempFilePath}"`);

  // Step 6: Reconstruction
  // Read formatted temp file
  const formattedRest = await readFile(tempFilePath, 'utf-8');

  // Reconstruct the file by putting back the formatted and resolved template
  const parts = formattedRest.split(placeholder);
  assert.ok(
      parts.length === 2,
      'Placeholder not found or found multiple times after formatting');

  const finalContent = parts[0] + formattedHtml + '\n' + parts[1];

  // Clean up temp file if it exists
  await unlink(tempFilePath);

  return finalContent;
}

async function main() {
  const parsed = parseArgs({
    allowPositionals: true,
    options: {
      'sort-attributes': {type: 'boolean', default: false},
      'dry-run': {type: 'boolean', default: false},
      'diff': {type: 'boolean', default: false},
    },
  });
  const inFiles = parsed.positionals;

  const isDryRun = parsed.values['dry-run'];
  const isDiff = parsed.values['diff'];
  let hasDiffAny = false;

  for (const f of inFiles) {
    const formattedContent =
        await formatFile(f, parsed.values['sort-attributes'], isDiff);
    if (formattedContent === null) {
      continue;
    }

    const originalContent = await readFile(f, 'utf-8');
    const hasDiff = originalContent !== formattedContent;

    if (isDryRun) {
      hasDiffAny ||= hasDiff;
    }

    if (!hasDiff) {
      if (!isDiff) {
        console.info(`${f} is already formatted.`);
      }
      continue;
    }

    if (isDryRun && !isDiff) {
      console.error(`Error: ${f} is not formatted.`);
      continue;
    }

    const writePath = isDiff ? `${f}.formatted.tmp.ts` : f;
    await writeFile(writePath, formattedContent, 'utf-8');

    if (isDiff) {
      try {
        const diffOutput = await execAsync(
            `python3 "${diffHelperPath}" "${f}" "${writePath}"`);
        stdout.write(diffOutput);
      } finally {
        await unlink(writePath);
      }
      continue;
    }

    console.info(`Successfully formatted and updated ${f}`);
  }

  if (hasDiffAny) {
    assert.ok(isDryRun);
    exit(2);
  }
}

main();
