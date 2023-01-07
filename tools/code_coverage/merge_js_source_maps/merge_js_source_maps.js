// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Merges 2 inline sourcemaps for a input file and writes a new
 * file to an output directory.
 */

import fs from 'fs';
import path from 'path';

import {ArgumentParser} from '../../../third_party/js_code_coverage/node_modules/argparse/argparse.js';
import {SourceMapConsumer, SourceMapGenerator} from '../../../third_party/js_code_coverage/node_modules/source-map/source-map.js';

/**
 * The prefix comment that indicates a data URL containing the sourcemap.
 */
const SOURCEMAPPING_DATA_URL_PREFIX =
    '//# sourceMappingURL=data:application/json;base64,';

/**
 * Decode a base64 encoded string representing a sourcemap to its utf-8
 * equivalent.
 * @param {string} contents Base64 encoded string.
 * @returns Decoded utf-8 string of the sourcemap.
 */
function decodeBase64SourceMap(contents) {
  const removedLeadingComment =
      contents.replace(SOURCEMAPPING_DATA_URL_PREFIX, '');
  const buf = Buffer.from(removedLeadingComment, 'base64');
  return buf.toString('utf-8');
}

/**
 * Helper to identify if a supplied line is an inline sourcemap.
 * @param {string} lineContents Contents of an individual line.
 * @returns True if line is an inline sourcemap, null otherwise.
 */
function isSourceMapComment(lineContents) {
  return lineContents && lineContents.startsWith(SOURCEMAPPING_DATA_URL_PREFIX);
}

/**
 * Convert `contents` into a valid dataURL sourceMappingURL comment.
 * @param {string} contents A string representation of the sourcemap
 * @returns A base64 encoded dataURL with the `SOURCEMAPPING_DATA_URL_PREFIX`
 *     prepended.
 */
function encodeBase64SourceMap(contents) {
  const buf = Buffer.from(contents, 'utf-8');
  return SOURCEMAPPING_DATA_URL_PREFIX + buf.toString('base64');
}

/**
 * Merge multiple sourcemaps to a single file.
 * @param {!Array<string>} sourceMaps An array of stringified sourcemaps.
 * @returns Returns a single sourcemap as a string.
 */
async function mergeSourcemaps(sourceMaps) {
  let generator = null;
  for await (const sourcemap of sourceMaps) {
    const parsedMap = JSON.parse(sourcemap);
    const consumer = await new SourceMapConsumer(parsedMap);
    if (generator) {
      generator.applySourceMap(consumer);
    } else {
      generator = await SourceMapGenerator.fromSourceMap(consumer);
    }
    consumer.destroy();
  }
  return generator.toString();
}

/**
 * Processes all input files for multiple inlined sourcemaps and merges them.
 * @param {!Array<string>} inputFiles The list of TS / JS files to extract
 *     sourcemaps from.
 */
async function processFiles(inputFiles, outputFiles) {
  for (let i = 0; i < inputFiles.length; i++) {
    const inputFile = inputFiles[i];
    const outputFile = outputFiles[i];
    const fileContents = fs.readFileSync(inputFile, 'utf-8');
    const inputLines = fileContents.split('\n');

    // Skip any trailing blank lines to find the last non-null line.
    let lastNonNullLine = inputLines.length - 1;
    while (inputLines[lastNonNullLine].trim().length === 0 &&
           lastNonNullLine > 0) {
      lastNonNullLine--;
    }

    // If the last non-null line identified is not a sourcemap, ignore this file
    // as it may have erroneously been marked for sourcemap merge.
    if (!isSourceMapComment(inputLines[lastNonNullLine])) {
      console.warn('Supplied file has no inline sourcemap', inputFile);
      fs.copyFileSync(inputFile, outputFile);
      continue;
    }

    // Extract out all the inline sourcemaps and decode them to their string
    // equivalent.
    const sourceMaps = [decodeBase64SourceMap(inputLines[lastNonNullLine])];
    let sourceMapLineIdx = lastNonNullLine - 1;
    while (isSourceMapComment(inputLines[sourceMapLineIdx]) &&
           sourceMapLineIdx > 0) {
      const sourceMap = decodeBase64SourceMap(inputLines[sourceMapLineIdx]);
      sourceMaps.push(sourceMap);
      sourceMapLineIdx--;
    }

    let mergedSourceMap = null;
    try {
      mergedSourceMap = await mergeSourcemaps(sourceMaps);
    } catch (e) {
      console.error(`Failed to merge inlined sourcemaps for ${inputFile}:`, e);
      fs.copyFileSync(inputFile, outputFile);
      continue;
    }

    // Drop off the lines that were previously identified as inline sourcemap
    // comments and replace them with the merged sourcemap.
    let finalFileContent =
        inputLines.slice(0, sourceMapLineIdx + 1).join('\n') + '\n';
    if (mergedSourceMap) {
      finalFileContent += encodeBase64SourceMap(mergedSourceMap);
    }
    fs.writeFileSync(outputFile, finalFileContent);
  }
}

async function main() {
  const parser =
      new ArgumentParser({description: 'Merge multiple inlined sourcemaps'});

  parser.add_argument('--sources', {help: 'Input files', nargs: '*'});
  parser.add_argument('--outputs', {help: 'Output files', nargs: '*'});
  parser.add_argument(
      '--manifest-files', {help: 'Output files', nargs: '*', required: false});

  const argv = parser.parse_args();
  await processFiles(argv.sources, argv.outputs);

  if (argv.manifest_files) {
    // TODO(crbug/1337530): Currently we just remove the final directory of the
    // `base_dir` key. This is definitely brittle and also subject to changes
    // made to the output directory. Consider updating this to be more robust.
    for (const manifestFile of argv.manifest_files) {
      try {
        const manifestFileContents =
            fs.readFileSync(manifestFile).toString('utf-8');
        const manifest = JSON.parse(manifestFileContents);
        manifest.base_dir = path.parse(manifest.base_dir).dir;
        const parsedPath = path.parse(manifestFile);
        fs.writeFileSync(
            path.join(
                parsedPath.dir,
                (parsedPath.name + '__processed' + parsedPath.ext)),
            JSON.stringify(manifest));
      } catch (e) {
        console.log(e);
      }
    }
  }
}

(async () => main())();