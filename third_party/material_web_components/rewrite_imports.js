// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This script resolves node imports to relative paths that can be consumed by
 * rollup. If this script becomes unmaintainable, consider using
 * @rollup/plugin-node-resolve instead.
 */

const path = require('path');
const resolve = require('resolve');
const fs = require('fs');
const { ArgumentParser } = require('argparse');

const parser = new ArgumentParser();
parser.add_argument('--basedir');
parser.add_argument('files', { nargs: '+' })
const args = parser.parse_args();
const inputFiles = args.files;
for (const inputFile of inputFiles) {
  const inputDir = path.dirname(inputFile);
  const data =
      fs.readFileSync(inputFile, {encoding: 'utf8'}).split('\n');

  // Investigate JS parsing if this is insufficient.
  const importRegex = /^((?:export [*{]|import ).*["'])([^.].*)(["'];)$/;
  const output = [];

  for (let line of data) {
    const match = line.match(importRegex);
    if (match) {
      const importPath = match[2];
      let resolved = resolve.sync(importPath, {basedir: args.basedir || inputDir});
      resolved = path.relative(inputDir, resolved);

      // Resolves to the module version of tslib since resolve.sync only
      // parses to the "main" field in the package.json.
      resolved = resolved.replace('tslib.js', 'tslib.es6.js');

      if (!resolved.startsWith('.')) {
        resolved = './' + resolved;
      }

      line = line.replace(importRegex, `$1${resolved}$3`);
    }
    output.push(line);
  }

  fs.writeFileSync(inputFile, output.join('\n'));
}
