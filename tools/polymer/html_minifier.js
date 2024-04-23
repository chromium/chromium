// Copyright 2022 The Chromium Authors
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

const assert = require('assert');
const fs = require('fs/promises');
const path = require('path');

// Regex to extract the CSS contents out of the HTML string. It matches anything
// that is wrapped by a '<style>...</style>' pair.
const REGEX = /^<style>(?<content>[\s\S]+)<\/style>$/;

async function processFile(inputFile, outputFile) {
  // Read file.
  let contents = await fs.readFile(inputFile, {encoding: 'utf8'});

  if (inputFile.endsWith('.css')) {
    // If this is a CSS file, wrap it with a <style> tag first, since
    // html-minifier only accepts HTML as input.
    contents = `<style>${contents}</style>`;
  }

  // Pass through html-minifier.
  let result = minify(contents, {
    caseSensitive: true,
    removeComments: true,
    minifyCSS: true,
  });

  if (inputFile.endsWith('.css')) {
    // If this is a CSS file, remove the <style>...</style> wrapper that was
    // added above.
    const match = result.match(REGEX);
    result = match === null ? '' : match.groups['content'];
  }

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
