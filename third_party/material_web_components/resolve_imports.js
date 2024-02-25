// Copyright 2021 The Chromium Authors
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
const acorn = require('acorn')

const parser = new ArgumentParser();
parser.add_argument('--basedir');
parser.add_argument('files', { nargs: '+' })
const args = parser.parse_args();
const inputFiles = args.files;

for (const inputFile of inputFiles) {
  const inputDir = path.dirname(inputFile);
  if (inputDir.startsWith("components-chromium/node_modules/@material")) continue;

  const data = fs.readFileSync(inputFile, {encoding: 'utf8'})
  let ast;
  try { 
    ast = acorn.parse(data, {sourceType: 'module', ecmaVersion: "latest"});
  } catch (e) {
    console.log(">> ACORN FAILED TO PARSE", inputFile);
    throw e;
  }

  const NODE_TYPES_TO_RESOLVE = [
    'ImportDeclaration',
    'ExportAllDeclaration',
    'ExportNamedDeclaration',
  ];

  const resolveNodes =
      ast.body.filter(n => NODE_TYPES_TO_RESOLVE.includes(n.type));
  const replacements = [];
  for (let i of resolveNodes) {
    const source = i.source;
    if (!source) {
      continue;
    }

    let resolved =
        resolve.sync(source.value, {basedir: args.basedir || inputDir});
    resolved = path.relative(inputDir, resolved);

    if (!resolved.startsWith('.')) {
      resolved = './' + resolved;
    }

    replacements.push({
      start: source.start,
      end: source.end,
      original: source.raw,
      replacement: `'${resolved}'`,
    });
  }

  const output = [];
  let curr = 0;
  for (const r of replacements) {
    output.push(data.substring(curr, r.start));
    output.push(r.replacement);
    curr = r.end;
  }
  output.push(data.substring(curr, data.length));
  fs.writeFileSync(inputFile, output.join(''));
}
