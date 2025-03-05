// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A wrapper around third_party/node/node_modules/terser/main.js
 * allowing minifying multiple input files and outputting them as separate files
 * in a single NodeJS invocation which is not possible with the command line
 * API.
 */
import assert from 'node:assert';
// Disable no-restricted-syntax to allow NodeJS imports which are extensionless.
// eslint-disable-next-line no-restricted-syntax
import {readFile, writeFile} from 'node:fs/promises';
import {join} from 'node:path';
import {parseArgs} from 'node:util';

import {minify} from '../../../../third_party/node/node_modules/terser/dist/bundle.min.js';

const CHROMIUM_REGEX = new RegExp(
    '\/\/ Copyright (?<year>\\d{4}) The Chromium Authors\n' +
    '\/\/ Use of this source code is governed by a BSD-style license that can be\n' +
    '\/\/ found in the LICENSE file.');
const THIRD_PARTY_REGEX = /\bCopyright\b(?!.*The Chromium Authors)/;

const COPYRIGHT_STRING = `// Copyright {minYear} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
`;

// Conslidate the file to only one Chromium Copyright header when applicable.
function consolidateChomiumCopyright(contents) {
  // Do not remove all Copyright headers if there is a non Chromium Copyright
  // header.
  const thirdPartyMatch = contents.match(THIRD_PARTY_REGEX);
  if (thirdPartyMatch === null) {
    // Iterate over all Chromium licence headers and extract the minimum year.
    const years = [];
    let chromiumMatch = null;
    while ((chromiumMatch = CHROMIUM_REGEX.exec(contents)) !== null) {
      years.push(Number.parseInt(chromiumMatch.groups['year']));
      // Remove license header.
      contents = contents.replace(CHROMIUM_REGEX, '');
    }

    if (years.length > 0) {
      const minYear = years.reduce((soFar, current) => {
        return Math.min(soFar, current);
      }, Number.POSITIVE_INFINITY);

      // Prepend with a single licence that uses the minimum year.
      contents = COPYRIGHT_STRING.replace('{minYear}', minYear) + contents;
    }
  }

  return contents;
}

async function main() {
  const options = {
    in_folder: {type: 'string'},
    out_folder: {type: 'string'},
  };
  const parsed = parseArgs({options, allowPositionals: true});
  const args = parsed.values;
  const inFiles = parsed.positionals;
  assert.ok(!!args['in_folder'], 'Missing required \'in_folder\' flag');
  assert.ok(!!args['out_folder'], 'Missing required \'out_folder\' flag');

  const terserOptions = {
    compress: false,
    mangle: false,
    module: true,
    format: {
      comments: '/Copyright|license|LICENSE/',
    },
  };

  async function minifyFile(file) {
    let contents =
        await readFile(join(args.in_folder, file), {encoding: 'utf-8'});
    contents = consolidateChomiumCopyright(contents);
    const result = await minify(contents, terserOptions);

    if (result.error) {
      console.error('minify_js:', result.error);
      return;
    }

    await writeFile(join(args.out_folder, file), result.code);
  }

  await Promise.all(inFiles.map(f => minifyFile(f)));
}
main();
