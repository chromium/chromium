/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A utility to write alltests.js for Closure Library.
 */

const {promises: fs} = require('fs');

const CLOSURE_PATH = 'closure/goog';
const THIRD_PARTY_PATH = 'third_party/closure/goog';

const HEADER = `
/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// This file has been auto-generated, please do not edit.
// To regenerate, run \`npm run gen_alltests_js\` in the root directory of the
// Closure Library git repository.
`.trim();

const FOOTER = `
// If we're running in a nodejs context, export tests. Used when running tests
// externally on Travis.
if (typeof module !== 'undefined' && module.exports) {
  module.exports = _allTests;
}
`.trim();

/**
 * Calls fs.readdir recursively on the given path and subdirectories.
 * Returns only the list of files.
 * @param {string} path The path to read.
 * @return {!Array<string>} A list of files.
 */
async function readdirRecursive(path) {
  const filesAndDirectories =
      (await fs.readdir(path))
          .map(fileOrDirectory => `${path}/${fileOrDirectory}`)
          .sort();
  const files = [];
  for (const fileOrDirectory of filesAndDirectories) {
    const fileStat = await fs.stat(fileOrDirectory);
    if (fileStat.isDirectory()) {
      files.push(...await readdirRecursive(fileOrDirectory));
    } else {
      files.push(fileOrDirectory);
    }
  }
  return files;
}

/**
 * Prints the generated alltests.js contents.
 * @return {!Promise<number>} The exit code.
 */
async function main() {
  try {
    // Get a list of all *_test.html files.
    const allTestHtmls = [
      ...await readdirRecursive(CLOSURE_PATH),
      ...await readdirRecursive(THIRD_PARTY_PATH),
    ].filter(f => f.endsWith('_test.html'));
    if (allTestHtmls.length === 0) {
      throw new Error(
          'No *_test.html files found. Did you run `npm run gen_test_htmls`?');
    }

    const output = [
      HEADER, '', 'var _allTests = [', ...allTestHtmls.map(f => `  '${f}',`),
      '];', '', FOOTER
    ];
    console.log(output.join('\n'));
    return 0;
  } catch (e) {
    console.error(e);
    return 1;
  }
}

main().then(code => process.exit(code));
