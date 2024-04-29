// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper script that performs some Polymer->Lit migration steps. See details
// below.

import fs from 'node:fs';
import path from 'node:path';
import {parseArgs} from 'node:util';

// Regular expression to extract CSS Content from within <style>...</style>
// tags. The 'd' flag is needed to obtain the start/end indices of the match.
const CSS_REGEX = /<style(?<deps>[\s\S][^>]{0,})>(?<content>[\s\S]+)<\/style>/d;

// Header to place on top of the newly created CSS file.
const CSS_FILE_HEADER = `/* Copyright 2024 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* #css_wrapper_metadata_start
 * #type=style-lit
 * #scheme=relative
 * #css_wrapper_metadata_end */
`;

const LISTENER_BINDING_REGEX =
    /on-(?<eventName>[a-zA-Z-]+)="(?<listenerName>[a-zA-Z_]+)"/g;

function processFile(file) {
  const basename = path.basename(file, '.ts');
  const htmlFile = path.join(path.dirname(file), basename + '.html');
  const cssFile = path.join(path.dirname(file), basename + '.css');

  // Step 1: Extract a standalone CSS file and write to disk.
  let htmlContent = fs.readFileSync(htmlFile, 'utf8');
  const match = htmlContent.match(CSS_REGEX);

  if (match !== null) {
    fs.writeFileSync(
        cssFile, CSS_FILE_HEADER + match.groups['content'], 'utf8');

    // Step 2: Remove <style>...</style> CSS content from HTML template file.
    htmlContent = htmlContent.substring(match.indices[0][1]);
  }

  // Step 3: Update event listeners syntax in HTML template
  htmlContent = htmlContent.replaceAll(
      LISTENER_BINDING_REGEX, function(_a, _b, _c, _d, _e, groups) {
        return `@${groups.eventName}="\${this.${groups.listenerName}}"`;
      });

  // Step 4: Update property access syntax in HTML template
  htmlContent = htmlContent.replaceAll(/\[\[!item/g, () => '${!item');
  htmlContent = htmlContent.replaceAll(/\[\[item/g, () => '${item');
  htmlContent = htmlContent.replaceAll(/\[\[!/g, () => '${!this.');
  htmlContent = htmlContent.replaceAll(/\[\[/g, () => '${this.');
  htmlContent = htmlContent.replaceAll(/\]\]/g, () => '}');

  // Step 5: Write updated HTML content to disk
  fs.writeFileSync(htmlFile, htmlContent, 'utf8');
}

function main() {
  const args = parseArgs({
                 options: {
                   file: {
                     type: 'string',
                   },
                 },
               }).values;

  processFile(args.file);
  console.log('DONE');
}
main();
