#!/usr/bin/env node

/**
 * Copyright 2023 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import {execSync, spawnSync} from 'child_process';
import {join, resolve} from 'path';

import {packageDirectorySync} from 'pkg-dir';

// Changing the current work directory to the package directory.
process.chdir(packageDirectorySync());

function log(message) {
  // eslint-disable-next-line no-console
  console.log(`(${process.argv[1]}) ${message}`);
}

function usage() {
  log(
    `Usage: [CHROMEDRIVER=<true | false>] [HEADLESS=<true | false>] [UPDATE_EXPECTATIONS=<true | false>] ${process.argv[1]} [webdriver/tests/bidi/[...]]`
  );
}

if (
  process.argv.length > 2 &&
  (process.argv.includes('-h') || process.argv.includes('--help'))
) {
  usage();
  process.exit(0);
}

let BROWSER_BIN = process.env.BROWSER_BIN;
if (!BROWSER_BIN) {
  BROWSER_BIN = execSync(
    `node ${join('tools', 'install-browser.mjs')} '--shell'`
  )
    .toString()
    .trim();
}

// Whether to use Chromedriver with mapper.
const CHROMEDRIVER = process.env.CHROMEDRIVER || 'false';

// Whether to start the server in headless or headful mode.
const HEADLESS = process.env.HEADLESS || 'true';

// The path to the WPT manifest file.
const MANIFEST = process.env.MANIFEST || 'MANIFEST.json';

// The browser product to test.
const PRODUCT = process.env.PRODUCT || 'chrome';

// Multiplier relative to standard test timeout to use.
const TIMEOUT_MULTIPLIER = process.env.TIMEOUT_MULTIPLIER || '4';

// Whether to update the WPT expectations after running the tests.
const UPDATE_EXPECTATIONS = process.env.UPDATE_EXPECTATIONS || 'false';

// Whether to enable verbose logging.
const VERBOSE = process.env.VERBOSE || 'false';

// The path to the WPT report file.
const WPT_REPORT = process.env.WPT_REPORT || 'wptreport.json';

// Only set WPT_METADATA if it's not already set.
const WPT_METADATA =
  process.env.WPT_METADATA ||
  join(
    'wpt-metadata',
    CHROMEDRIVER === 'true' ? 'chromedriver' : 'mapper',
    HEADLESS === 'true' ? 'headless' : 'headful'
  );

if (HEADLESS === 'true') {
  log('Running WPT in headless mode...');
} else {
  log('Running WPT in headful mode...');
}

const wptRunArgs = [
  '--binary',
  BROWSER_BIN,
  `--webdriver-arg=--headless=${HEADLESS}`,
  '--log-wptreport',
  WPT_REPORT,
  '--manifest',
  MANIFEST,
  '--metadata',
  WPT_METADATA,
  '--no-manifest-download',
  '--skip-implementation-status',
  'backlog',
  '--timeout-multiplier',
  TIMEOUT_MULTIPLIER,
  '--run-by-dir',
  '1',
];

if (VERBOSE === 'true') {
  // WPT logs.
  wptRunArgs.push(
    '--debug-test',
    '--log-mach',
    '-',
    '--log-mach-level',
    'info'
  );
}

if (CHROMEDRIVER === 'true') {
  log('Using chromedriver with mapper...');
  if (HEADLESS === 'true') {
    wptRunArgs.push('--binary-arg=--headless=new');
  }
  wptRunArgs.push(
    '--install-webdriver',
    `--webdriver-arg=--bidi-mapper-path=${join('lib', 'iife', 'mapperTab.js')}`,
    `--webdriver-arg=--log-path=${join('out', 'chromedriver.log')}`,
    `--webdriver-arg=--log-level=${VERBOSE === 'true' ? 'ALL' : 'INFO'}`,
    '--yes'
  );
} else {
  wptRunArgs.push(
    `--webdriver-arg=--headless=${HEADLESS}`,
    '--webdriver-binary',
    join('tools', 'run-bidi-server.mjs')
  );
  log('Using pure mapper...');
}

const restArgs = process.argv.slice(2);
let test = restArgs[restArgs.length - 1] ?? join('webdriver', 'tests', 'bidi');

// Canonicalize the test path.
test = test
  .replace('wpt-metadata/', '')
  .replace('mapper/headless/', '')
  .replace('mapper/headful/', '')
  .replace('chromedriver/headless/', '')
  .replace('.ini', '');

log(`Running "${test}" with "${BROWSER_BIN}"...`);

wptRunArgs.push(
  // All arguments except the first one (the command) and the last one (the test) are the flags.
  ...process.argv.slice(2, process.argv.length - 1),
  PRODUCT,
  // The last argument is the test.
  test
);

const wptBinary = resolve(join('wpt', 'wpt'));
const {status} = spawnSync(wptBinary, ['run', ...wptRunArgs], {
  stdio: 'inherit',
});

if (UPDATE_EXPECTATIONS === 'true') {
  log('Updating WPT expectations...');

  spawnSync(
    wptBinary,
    [
      'update-expectations',
      '--properties-file',
      './update_properties.json',
      '--product',
      PRODUCT,
      '--manifest',
      MANIFEST,
      '--metadata',
      WPT_METADATA,
      WPT_REPORT,
    ],
    {stdio: 'inherit'}
  );
}

process.exit(status ?? 0);
