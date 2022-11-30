/**
 * @license
 * Copyright 2018 The Closure Library Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS-IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const {execute} = require('../../bin/closuremakedeps');
const path = require('path');
const fs = require('fs');
const jasmineDiff = require('jasmine-diff');
const os = require('os');

jasmine.DEFAULT_TIMEOUT_INTERVAL = 100000;

// By default assume this is running based on the structure of the git repo.
// closure_deps/ lives in the save repo as Closure, and the library's code is in
// closure/goog and third_party/closure/goog.
const CLOSURE_SUB_DIR = process.env.CLOSURE_SUB_DIR || 'closure/goog';
const CLOSURE_LIBRARY_PATH =
    process.env.CLOSURE_LIBRARY_PATH || path.resolve(__dirname, '../../../');

const CLOSURE_PATH = path.resolve(CLOSURE_LIBRARY_PATH, CLOSURE_SUB_DIR);
const THIRD_PARTY_PATH =
    path.resolve(CLOSURE_LIBRARY_PATH, 'third_party', CLOSURE_SUB_DIR);

/**
 * @param {string} flag
 * @param {string} value
 * @return {!Array<string>}
 */
function flag(flag, value) {
  return [flag, value];
}

/**
 * Sorts the lines of a string, since the specific order of the addDependency
 * lines is not strictly important. TODO(sdh): ideally we would not need to
 * do this, but due to some internal work moving these files, we end up with
 * a different order.  A better fix would be to simply use the same tool and
 * then the golden tests would be unnecessary.
 *
 * @param {string} str
 * @return {string}
 */
function sortLines(str) {
  return str.split('\n').sort().join('\n');
}

describe('closure-make-deps', function() {
  const tempFile = path.join(os.tmpdir(), 'closuremakejsdepstmp.js');
  const tempFileRelativePath = path.relative(CLOSURE_PATH, tempFile);

  const closureDepsContents =
      fs.readFileSync(path.resolve(CLOSURE_PATH, 'deps.js'), {
          encoding: 'utf8'
        }).replace(/^(\/\/.*)?\n/gm, '');

  beforeEach(function() {
    jasmine.addMatchers(jasmineDiff(jasmine, {
      colors: false,
      inline: false,
    }));
  });

  afterEach(() => {
    if (fs.existsSync(tempFile)) {
      fs.unlinkSync(tempFile);
    }
  });

  it('merge only deps produces same file', async function() {
    const flags = [
      ...flag('--merge-deps', 'true'),
      ...flag('--closure-path', CLOSURE_PATH),
      ...flag('-f', path.join(CLOSURE_PATH, 'deps.js')),
    ];

    const result = await execute(flags);
    expect(result.errors).toEqual([]);
    expect(sortLines(result.text)).toEqual(sortLines(closureDepsContents));
  });

  it('input deps file forward declares symbols', async function() {
    fs.writeFileSync(
        tempFile, `goog.module('ex');\ngoog.require('goog.array');`);

    const flags = [
      ...flag('--closure-path', CLOSURE_PATH),
      ...flag('-f', path.join(CLOSURE_PATH, 'deps.js')),
      ...flag('-f', tempFile)
    ];

    const result = await execute(flags);
    expect(result.errors).toEqual([]);
    expect(result.text)
        .toEqual(
            `goog.addDependency('${tempFileRelativePath}'` +
            `, ['ex'], ['goog.array'], {'module': 'goog'});\n`);
  });

  it('missing require is error', async function() {
    fs.writeFileSync(tempFile, `goog.require('goog.array');`);

    const flags = [
      ...flag('--closure-path', CLOSURE_PATH),
      ...flag('-f', tempFile)
    ];

    try {
      await execute(flags);
      fail();
    } catch (error) {
      expect(error.toString())
          .toEqual(
              `Error: Error in source file "${tempFile}": ` +
              `Could not find "goog.array".`);
    }
  });

  it('merge deps and input file', async function() {
    fs.writeFileSync(tempFile, `goog.module('a.b');`);

    const expectedExtraFileLine = `goog.addDependency('${
        tempFileRelativePath}', ['a.b'], [], {'module': 'goog'});\n`;

    const expectedContents = expectedExtraFileLine + closureDepsContents;

    const flags = [
      ...flag('--merge-deps', 'true'),
      ...flag('--closure-path', CLOSURE_PATH),
      ...flag('-f', tempFile),
      ...flag('-f', path.join(CLOSURE_PATH, 'deps.js')),
    ];

    const result = await execute(flags);
    expect(result.errors).toEqual([]);
    expect(sortLines(result.text)).toEqual(sortLines(expectedContents));
  });
});
