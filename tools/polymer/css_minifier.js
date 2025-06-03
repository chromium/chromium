// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A small wrapper around
 * third_party/node/node_modules/postcss and
 * third_party/node/node_modules/postcss-minify to process an explicit list of
 * HTML or CSS input files, by minifying their CSS contents (removing any blank
 * space and comments, no other transformations made).
 */
const postcss =
    require('../../third_party/node/node_modules/postcss/lib/postcss.js');
const postcssMinify =
    require('../../third_party/node/node_modules/postcss-minify/index.js');

const assert = require('assert');
const fs = require('fs/promises');
const path = require('path');

// Regular expression to extract CSS content from within <style>...</style> or
// <style include="...">...</style> tags. The 'd' flag is needed to obtain the
// start/end indices of the 'content' captured group.
const REGEX = /<style([^>]*)?>(?<content>[^<]+)<\/style>/d;

async function processCssFile(inputFile, outputFile) {
  // Read file.
  const contents = await fs.readFile(inputFile, {encoding: 'utf8'});

  // Pass through postcss-minify.
  const result = await postcss([
                   postcssMinify
                 ]).process(contents, {from: undefined, to: undefined});

  // Save result.
  await fs.mkdir(path.dirname(outputFile), {recursive: true});
  // Strip any trailing new line character.
  return fs.writeFile(
      outputFile, result.css.replace(/\n$/, ''), {enconding: 'utf8'});
}

async function processHtmlFile(inputFile, outputFile) {
  // Read file.
  const contents = await fs.readFile(inputFile, {encoding: 'utf8'});
  let minifiedContents = contents;

  // Extract the inlined <style>...</style> contents.
  const match = contents.match(REGEX);
  if (match !== null) {
    // Pass through postcss-minify.
    const result =
        await postcss([
          postcssMinify
        ]).process(match.groups['content'], {from: undefined, to: undefined});


    // Replace the original inlined <style>...</style> contents with the
    // minified ones.
    const indices = match.indices.groups['content'];
    minifiedContents = contents.substring(0, indices[0]) +
        // Strip any trailing new line character.
        result.css.replace(/\n$/, '') + contents.substring(indices[1]);
  }

  // Save result.
  await fs.mkdir(path.dirname(outputFile), {recursive: true});
  return fs.writeFile(outputFile, minifiedContents, {enconding: 'utf8'});
}

function main() {
  const args = {
    inputDir: process.argv[2],
    outputDir: process.argv[3],
    inputFiles: process.argv.slice(4),
  }

  for (const f of args.inputFiles) {
    assert(f.endsWith('.html') || f.endsWith('.css'));
    if (f.endsWith('.html')) {
      processHtmlFile(
          path.join(args.inputDir, f), path.join(args.outputDir, f));
    } else if (f.endsWith('.css')) {
      processCssFile(path.join(args.inputDir, f), path.join(args.outputDir, f));
    }
  }
}
main();
