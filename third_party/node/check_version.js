// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {parseArgs} from 'node:util';
import process from 'node:process';

function extractExpectedVersion(contents) {
  const NODE_VERSION_REGEX = /\s*NODE_VERSION\s*=\s*"(?<version>[^"]+)"/;
  const match = contents.match(NODE_VERSION_REGEX);
  assert.ok(match !== null, 'Could not extract NodeJS version.');
  return match.groups['version'];
}

async function main() {
  const options = {
    expected_version_file: {type: 'string'},
  };
  const parsed = parseArgs({options});
  const args = parsed.values;
  assert.ok(
      !!args.expected_version_file,
      'Missing required \'expected_version_file\' flag');

  let contents =
      await readFile(args.expected_version_file, {encoding: 'utf-8'});
  const expectedVersion = extractExpectedVersion(contents);

  const errorMessage =
      `Failed NodeJS version check: Expected version '${expectedVersion}', ` +
      `but found '${process.version}'. Did you run 'gclient sync'? If the ` +
      `problem persists try running 'gclient sync -f' instead, or deleting ` +
      `third_party/node/{linux,win,mac} folders and trying again.`;

  assert.equal(expectedVersion, process.version, errorMessage);
}
main();
