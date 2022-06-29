// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A small wrapper around
 * third_party/node/node_modules/html-minifier, allowing processing an explicit
 * list of input files, which is not supported by the built-in CLI of
 * html-minifier.
 */
const minify =
    require(
        '../../third_party/node/node_modules/html-minifier/src/htmlminifier.js')
        .minify;

const path = require('path');
const fs = require('fs/promises');

async function processFile(inputFile, outputFile) {
  // Read file.
  const contents = await fs.readFile(inputFile, {encoding: 'utf8'});

  // Pass through html-minifier.
  const result = minify(contents, {
    removeComments: true,
    minifyCSS: true,
  });

  // Save result.
  await fs.mkdir(path.dirname(outputFile), {recursive: true});
  return fs.writeFile(outputFile, result, {enconding: 'utf8'});
}

function main() {
  const args = {
    inputDir: process.argv[2],
    outputDir: process.argv[3],
    inputFiles: process.argv.slice(4),
  }

  for (const f of args.inputFiles) {
    processFile(path.join(args.inputDir, f), path.join(args.outputDir, f));
  }
}
main();
