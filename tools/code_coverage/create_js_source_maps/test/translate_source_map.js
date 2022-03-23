// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple wrapper around mozilla/source-map. Takes a source map
 * and a location in the post-processed file and prints the corresponding
 * location in the pre-processing file.
 *
 * Helper for create_js_source_maps_test.py.
 */
import fs from 'fs';

import {SourceMapConsumer} from '../../../../third_party/js_code_coverage/node_modules/source-map/source-map.js';
// TODO(crbug.com/1307980): Move argparse to the js_code_coverage library.
import {ArgumentParser} from '../../../../third_party/node/node_modules/argparse/index.js';

const parser = new ArgumentParser({
  description: 'Applies a JavaScript sourcemap to a line and column number'
});

parser.addArgument(
    '--source_map',
    {help: 'Source map to use for translation', required: true});
parser.addArgument(
    '--line',
    {help: 'Line number in post-processed file', type: 'int', required: true});
parser.addArgument('--column', {
  help: 'Column number in post-processed file',
  type: 'int',
  required: true
});

const argv = parser.parseArgs();


const sourceMap = JSON.parse(fs.readFileSync(argv.source_map));
// Async function to get around "Cannot use keyword 'await' outside an async
// function" complaint in ESLint. Our version of node would allow us to use
// 'await' at the top level, but our version of ESLint fails.
(async function() {
  const consumer = await new SourceMapConsumer(sourceMap);
  const result =
      consumer.originalPositionFor({line: argv.line, column: argv.column});
  console.log(JSON.stringify(result));
  consumer.destroy();
}());
