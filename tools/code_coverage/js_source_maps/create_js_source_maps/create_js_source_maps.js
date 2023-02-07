// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple wrapper around mozilla/source-map. Scans a file
 * processed by preprocess_if_expr.py looking for erasure comments. It creates
 * a sourcemap mapping the post-processed TypeScript or JavaScript back to the
 * original TypeScript or JavaScript.
 */

import fs from 'fs';
import path from 'path';

import {ArgumentParser} from '../../../../third_party/js_code_coverage/node_modules/argparse/argparse.js';
import {SourceMapGenerator} from '../../../../third_party/js_code_coverage/node_modules/source-map/source-map.js';

// Regex matching the comment indicating that preprocess_if_expr removed lines.
// The capture group contains the number of lines removed. Must match the
// comment added by tools/grit/grit/format/html_inline.py
const GRIT_REMOVED_LINES_REGEX = /grit-removed-lines:(\d+)/g;

/**
 * Processes the TypeScript or JavaScript files and produces
 * source map file / appends a source map.
 *
 * @param {Array} originals Original paths of `inputs`.
 * @param {Array} inputs The TypeScript or JavaScript files to read from.
 * @param {Array} outputs If `inlineSourcemaps`, the output TypeScript
 *                         or JavaScript files with the appended source map.
 *                         Otherwise, the standalone map file.
 * @param {boolean} verbose If true, print detailed information about the
 *                          mappings as they are added.
 * @param {string} sourceRoot The root for all source files.
 * @param {boolean} inlineSourcemaps If true, append source map instead of
 *                                   creating standalone map file.
 */
function processAllFiles(
    originals, inputs, outputs, verbose, sourceRoot, inlineSourcemaps) {
  if (originals.length != inputs.length || originals.length != outputs.length) {
    console.error(
        `The original files count(${originals.length}), ` +
        `the input files count(${inputs.length}) and the ` +
        `output files count(${outputs.length}) should be the same.`);
    process.exit(1);
  }

  for (let i = 0; i < originals.length; i++) {
    processOneFile(
        originals[i], inputs[i], outputs[i], verbose, sourceRoot,
        inlineSourcemaps);
  }
}

/**
 * Adds a mapping for a line. We only map lines, not columns -- we don't have
 * enough information to map columns within a line. (And the usual usage of
 * preprocess_if_expr means we don't expect to see partial line removals with
 * code after the removal.)
 *
 * @param {SourceMapGenerator} map The SourceMapGenerator.
 * @param {string} sourceFileName The name of the original file.
 * @param {number} originalLine The current line in the original source file.
 * @param {number} generatedLine The current line in the generated (processed)
 *                               source file.
 * @param {boolean} verbose If true, print detailed information about the
 *                          mappings as they are added.
 */
function addMapping(map, sourceFileName, originalLine, generatedLine, verbose) {
  const mapping = {
    source: sourceFileName,
    original: {
      line: originalLine,
      column: 0,
    },
    generated: {
      line: generatedLine,
      column: 0,
    },
  };
  if (verbose) {
    console.info(mapping);
  }
  map.addMapping(mapping);
}

/**
 * Processes one processed TypeScript or JavaScript file and produces one
 * source map file / appends a source map.
 *
 * @param {string} originalFileName Original path of `inputFileName`.
 * @param {string} inputFileName The TypeScript or JavaScript file to read from.
 * @param {string} outputFileName If `inlineSourcemaps`, the output TypeScript
 *                                or JavaScript file with the append source map.
 *                                Otherwise, the standalone map file.
 * @param {boolean} verbose If true, print detailed information about the
 *                          mappings as they are added.
 * @param {string} sourceRoot The root for all source files.
 * @param {boolean} inlineSourcemaps If true, append source map instead of
 *                                   creating standalone map file.
 */
function processOneFile(
    originalFileName, inputFileName, outputFileName, verbose, sourceRoot,
    inlineSourcemaps) {
  const inputFile = fs.readFileSync(inputFileName, 'utf8');
  const inputLines = inputFile.split('\n');
  const map = new SourceMapGenerator(
      {file: path.resolve(outputFileName), sourceRoot: sourceRoot});

  let originalLine = 0;
  let generatedLine = 0;

  for (const line of inputLines) {
    generatedLine++;
    originalLine++;

    // Add to sourcemap before looking for removal comments. The beginning of
    // the generated line came from the parts before the removal comment.
    addMapping(map, originalFileName, originalLine, generatedLine, verbose);

    for (const removal of line.matchAll(GRIT_REMOVED_LINES_REGEX)) {
      const removedLines = Number.parseInt(removal[1], 10);
      if (verbose) {
        console.info(`Found grit-removed-lines:${removedLines} on line ${
            generatedLine}`);
      }
      originalLine += removedLines;
    }
  }

  // Inline the source content.
  map.setSourceContent(
      originalFileName, fs.readFileSync(originalFileName).toString());

  if (!inlineSourcemaps) {
    fs.writeFileSync(outputFileName, map.toString());
  } else {
    const mapBase64 = Buffer.from(map.toString()).toString('base64');
    const output =
        `${inputFile}\n//# sourceMappingURL=data:application/json;base64,${
            mapBase64}`;
    fs.writeFileSync(outputFileName, output);
  }
}

function main() {
  const parser = new ArgumentParser({
    description:
        'Creates source maps for files preprocessed by preprocess_if_expr',
  });

  parser.addArgument(
      ['-v', '--verbose'],
      {help: 'Print each mapping & removed-line comment', action: 'storeTrue'});
  parser.addArgument(['--inline-sourcemaps'], {
    help: 'Copies contents of input to output and appends inline source maps',
    action: 'storeTrue',
  });
  parser.addArgument(
      '--originals', {help: 'Original file name', nargs: '*', action: 'store'});
  parser.addArgument(
      '--inputs', {help: 'Input file name', nargs: '*', action: 'store'});
  parser.addArgument(
      '--outputs', {help: 'Output file name', nargs: '*', action: 'store'});
  parser.addArgument('--sourceRoot', {
    help: 'Source directory to store the source map',
    required: false,
    default: process.cwd(),
  });

  const argv = parser.parseArgs();

  processAllFiles(
      argv.originals, argv.inputs, argv.outputs, argv.verbose, argv.sourceRoot,
      argv.inline_sourcemaps);
}

main();
