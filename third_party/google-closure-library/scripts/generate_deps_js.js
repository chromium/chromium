/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A utility to write deps.js for Closure Library.
 */

const {closureMakeDeps} = require('google-closure-deps');

const CLOSURE_PATH = 'closure/goog';
const THIRD_PARTY_PATH = 'third_party/closure/goog';

const HEADER = `
// Copyright The Closure Library Authors.
// SPDX-License-Identifier: Apache-2.0

// This file has been auto-generated, please do not edit.
// To regenerate, run \`npm run gen_deps_js\` in the root directory of the
// Closure Library git repository.

// Disable Clang formatter for this file.
// See http://goo.gl/SdiwZH
// clang-format off
`.trim();

/**
 * Flags to add to closureMakeDeps in order to exclude tests.
 */
const TEST_EXCLUSION_FLAGS = [
  '--exclude',
  '**/*_test.js',
  '--exclude',
  '**/tester.js',
  '--exclude',
  '**/*_test_vectors.js',
  '--exclude',
  '**/*_test_suite.js',
  '--exclude',
  '**/*_test_cases.js',
  '--exclude',
  '**/testdata/**/*.js',
  '--exclude',
  `${CLOSURE_PATH}/demos/`,
];

/**
 * Prints the generated deps.js contents.
 * @param {!Array<string>} args Command-line arguments.
 * @return {!Promise<number>} The exit code.
 */
async function main(args) {
  try {
    const genDepsWithTests = args.indexOf('--with_tests') != -1;
    const {text, errors} = await closureMakeDeps.execute([
      '--root',
      CLOSURE_PATH,
      '--root',
      THIRD_PARTY_PATH,
      ...genDepsWithTests ? [] : TEST_EXCLUSION_FLAGS,
      '--exclude',
      `${CLOSURE_PATH}/deps*.js`,
      '--exclude',
      `${CLOSURE_PATH}/transpile.js`,
    ]);
    // Print all encountered errors. Errors are not necessarily fatal.
    for (const error of errors) {
      console.error(error.toString());
    }
    if (text) {
      console.log(`${HEADER}\n\n${text.trim()}`);
      return 0;
    } else {
      // No text indicates that the errors were fatal.
      return 1;
    }
  } catch (e) {
    console.error(e);
    return 1;
  }
}

main(process.argv.slice(2)).then(code => process.exit(code));
