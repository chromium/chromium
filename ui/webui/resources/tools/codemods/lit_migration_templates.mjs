// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper script that performs some Polymer->Lit migration steps. See details
// below.

import fs from 'node:fs';
import path from 'node:path';
import {parseArgs} from 'node:util';

// Regular expression to extract CSS Content from within <style>...</style> or
// <style include="...">...</style> tags. The 'd' flag is needed to obtain the
// start/end indices of the match.
const CSS_REGEX = /<style(?<deps>[^>]*)?>(?<content>[^]*?)<\/style>/d;

const CSS_IMPORT_REGEX = /import '(?<path>[^']+)\.css\.js';?\n/g;

const LIT_MIGRATION_STYLES_METHOD_REGEX =
    /  static override get styles\(\) \{\n    return getCss\(\);\n  \}/;

// Header to place on top of the newly created CSS file.
const CSS_FILE_HEADER = `/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* #css_wrapper_metadata_start
 * #type=style-lit
 * #scheme=relative
`;

const CSS_FILE_HEADER_END = ` * #css_wrapper_metadata_end */
`;

const HTML_TS_FILE_HEADER = `// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ELEMENT_TYPE_PLACEHOLDER} from './FILE_PATH_PLACEHOLDER.js';

export function getHtml(this: ELEMENT_TYPE_PLACEHOLDER) {
  // clang-format off
  return html\`<!--_html_template_start_-->
`;

const HTML_TS_FILE_FOOTER = `
<!--_html_template_end_-->\`;
  // clang-format on
}
`;

const LISTENER_BINDING_REGEX =
    /on-(?<eventName>[a-zA-Z-]+)="(?<listenerName>[a-zA-Z0-9_]+)"/g;

// Regular expression to parse 2-way bindings like value="{{myValue_}}",
// and extract 'value' and 'myValue_' into captured groups for further
// processing.
const LISTENER_BINDING_TWO_WAY_REGEX =
    /(?<childProp>[a-z-]+)="\{\{(?<parentProp>[a-zA-Z0-9_]+)\}\}"/g;

// Regular expression to extract a references to TS methods or member variables
// in HTML templates. For example 'foo' will be extracted from ${this.foo} or
// ${this.foo_(abc)}.
const TS_REFERENCE_REGEX = /\$\{this\.(?<reference>[^(}]+)(\([^)]*\)){0,1}}/g;

const TS_CLASSNAME_REGEX =
    /class (?<className>[a-zA-Z0-9]*) extends (CrLitElement|.*ElementBase)/;

// Replaces part of a string with a the provided replacement string.
function replaceRange(string, start, end, replacement) {
  return string.substring(0, start) + replacement + string.substring(end);
}

function toCamelCase(str) {
  return str.replace(/-([a-z])/g, g => g[1].toUpperCase())
      .replace(/^[a-z]/, g => g[0].toUpperCase());
}

// Processes the case where the style found in the HTML file contains included
// shared styles but no local content.
function handleEmptyStyleTag(tsContent, polymerDeps, basename) {
  const aliasedGets = [];
  let newImports = '';
  for (const dep of polymerDeps) {
    const underscoreDep = dep.replaceAll('-', '_');
    const alias = `get${toCamelCase(dep)}Css`;
    aliasedGets.push(`${alias}()`);

    // Find the import for this dep and replace it with an aliased getCss
    // import.
    const depImportRegex = new RegExp(
        `import '(?<path>[^']*(?<filename>${underscoreDep}))\\.css\\.js';?\\n`);
    const depMatch = tsContent.match(depImportRegex);
    if (depMatch) {
      const basePath = depMatch.groups['path'] + '_lit';
      tsContent = tsContent.replace(
          depMatch[0],
          `import {getCss as ${alias}} from '${basePath}.css.js';\n`);
    } else {
      // Handle the case where the original Polymer file is missing the
      // correct import by adding a placeholder import and a TODO.
      const stylePath = 'STYLE_PATH';  // Placeholder
      newImports +=
          `// TODO: identify the correct path and replace the placeholder.\n` +
          `import {getCss as ${alias}} from '${stylePath}/${
              underscoreDep}_lit.css.js';\n`;
    }
  }

  const litMigrationCssImport =
      `import {getCss} from './${basename}.css.js';\n`;
  // Remove the local getCss import added by lit_migration.js, and replace with
  // any missing shared style imports.
  tsContent = tsContent.replace(litMigrationCssImport, newImports);

  // Replace the single getCss() call added by lit_migration.js with an array
  // of shared style calls.
  const newStylesMethod = `  static override get styles() {
    return [
      ${aliasedGets.join(',\n      ')},
    ];
  }`;

  return tsContent.replace(LIT_MIGRATION_STYLES_METHOD_REGEX, newStylesMethod);
}

// Generates the header for the newly created standalone CSS file.
function getCssFileHeader(litDeps, cssImports) {
  const importStrings = cssImports.map(importMatch => {
    const path = importMatch.groups['path'];
    return path.endsWith('vars') ?
        ` * #import=${path}.css.js` : ` * #import=${path}_lit.css.js`;
  });

  let header = CSS_FILE_HEADER;
  if (litDeps) {
    header += ` * #include=${litDeps}\n`;
  }
  if (importStrings.length > 0) {
    header += importStrings.join('\n') + '\n';
  }
  header += CSS_FILE_HEADER_END;
  return header;
}

function processFile(file, outputHtml) {
  const basename = path.basename(file, '.ts');
  const tsFile = path.join(path.dirname(file), basename + '.ts');
  const htmlFile = path.join(path.dirname(file), basename + '.html');
  const htmlTsFile = path.join(path.dirname(file), basename + '.html.ts');
  const cssFile = path.join(path.dirname(file), basename + '.css');

  let tsContent = fs.readFileSync(tsFile, 'utf8');

  // Step 1: Extract a standalone CSS file.
  let htmlContent = fs.readFileSync(htmlFile, 'utf8');
  const match = htmlContent.match(CSS_REGEX);

  if (match !== null) {
    // Extract the deps from the <style> tag and convert them to Lit styles.
    const deps = match.groups['deps'];
    let litDeps = '';
    let polymerDeps = [];
    if (deps) {
      let depString = deps.slice(deps.indexOf('"') + 1);
      depString = depString.slice(0, depString.indexOf('"')).trim();
      polymerDeps = depString.split(' ');
      litDeps = polymerDeps.map(dep => dep + '-lit').join(' ');
    }

    const content = match.groups['content'];
    if (content.trim() === '' && polymerDeps.length > 0) {
      tsContent = handleEmptyStyleTag(tsContent, polymerDeps, basename);
    } else {
      const header = getCssFileHeader(
          litDeps, Array.from(tsContent.matchAll(CSS_IMPORT_REGEX)));
      fs.writeFileSync(cssFile, header + content, 'utf8');
    }

    // Always remove any remaining .css.js imports from the .ts file.
    tsContent = tsContent.replaceAll(CSS_IMPORT_REGEX, '');

    // Step 2: Remove <style>...</style> content from HTML template file.
    htmlContent = htmlContent.substring(match.indices[0][1]);
  }

  // Step 3: Update event listeners syntax in HTML template
  htmlContent = htmlContent.replaceAll(
      LISTENER_BINDING_REGEX, function(_a, _b, _c, _d, _e, groups) {
        return `@${groups.eventName}="\${this.${groups.listenerName}}"`;
      });

  // Step 4: Update property access syntax in HTML template (1-way bindings)
  htmlContent = htmlContent.replaceAll(/\[\[!item/g, () => '${!item');
  htmlContent = htmlContent.replaceAll(/\[\[item/g, () => '${item');
  htmlContent = htmlContent.replaceAll(/\[\[!/g, () => '${!this.');
  htmlContent = htmlContent.replaceAll(/\[\[/g, () => '${this.');
  htmlContent = htmlContent.replaceAll(/\]\]/g, () => '}');

  // Step 5: Update property access syntax in HTML template (2-way bindings)
  const matches =
      Array.from(htmlContent.matchAll(LISTENER_BINDING_TWO_WAY_REGEX));
  // Reverse the order so that the character indices don't get messed up after
  // modifying the original string, effectively processing the matches from the
  // end of the string to the start.
  matches.reverse();

  // For each match, change
  // value="{{myValue_}}"
  // to
  // value="${this.myValue_}" @value-changed="${this.onMyValueChanged_}"
  for (let i = 0; i < matches.length; i++) {
    const g = matches[i].groups;
    let listenerPart =
        g['parentProp'].charAt(0).toUpperCase() + g['parentProp'].slice(1);
    listenerPart = listenerPart.replace('_', '');
    const listener =
        `@${g['childProp']}-changed="\${this.on${listenerPart}Changed_}"`
    const binding = matches[i][0].replace('{{', '${this.').replace('}}', '}');
    const start = matches[i].index;
    const end = matches[i].index + matches[i][0].length;
    htmlContent =
        replaceRange(htmlContent, start, end, `${binding} ${listener}`);
  }

  if (outputHtml) {
    // Step 6: Write .html file with updated HTML content to disk.
    fs.writeFileSync(htmlFile, htmlContent, 'utf8');
  } else {
    // Add the .html.ts file header and footer sections, making replacements
    // for the import path and class name.
    let htmlTsHeader =
        HTML_TS_FILE_HEADER.replace('FILE_PATH_PLACEHOLDER', basename);
    const classNameMatch = tsContent.match(TS_CLASSNAME_REGEX);
    if (classNameMatch) {
      htmlTsHeader = htmlTsHeader.replaceAll(
          'ELEMENT_TYPE_PLACEHOLDER', classNameMatch.groups['className']);
    }

    // Step 6: Write .html.ts file with updated HTML content and wrapper
    // file content to disk.
    fs.writeFileSync(
        htmlTsFile, htmlTsHeader + htmlContent + HTML_TS_FILE_FOOTER, 'utf8');
  }

  // Step 7: Extract all methods/variables being referenced from the template
  //         and if they are 'private' change them to 'protected'.
  const references =
      Array.from(htmlContent.matchAll(TS_REFERENCE_REGEX)).map(m => m[1]);
  for (const ref of references) {
    tsContent = tsContent.replaceAll(`private ${ref}`, `protected ${ref}`);
  }

  // Step 8: Write updated TS content to disk.
  fs.writeFileSync(tsFile, tsContent, 'utf8');
}

function main() {
  const args = parseArgs({
                 options: {
                   file: {
                     type: 'string',
                   },
                   outputHtml: {
                     type: 'boolean',
                   },
                 },
               }).values;

  processFile(args.file, args.outputHtml);
  console.log('DONE');
}
main();
