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

import {spawnSync} from 'child_process';
import {cpus} from 'os';
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

const isLinux = process.platform === 'linux';
const isMacOS = process.platform === 'darwin';

// The path to the browser binary.
const BROWSER_BIN =
  process.env.BROWSER_BIN ||
  (isLinux
    ? '/usr/bin/google-chrome-unstable'
    : isMacOS
    ? '/Applications/Google Chrome Dev.app/Contents/MacOS/Google Chrome Dev'
    : '');

// Whether to use Chromedriver with mapper.
const CHROMEDRIVER = process.env.CHROMEDRIVER || 'false';

// Whether to start the server in headless or headful mode.
const HEADLESS = process.env.HEADLESS || 'true';

// The path to the WPT manifest file.
const MANIFEST = process.env.MANIFEST || 'MANIFEST.json';

// The browser product to test.
const PRODUCT = process.env.PRODUCT || 'chrome';

// Multiplier relative to standard test timeout to use.
const TIMEOUT_MULTIPLIER = process.env.TIMEOUT_MULTIPLIER || '8';

// Whether to update the WPT expectations after running the tests.
const UPDATE_EXPECTATIONS = process.env.UPDATE_EXPECTATIONS || 'false';

// Whether to enable verbose logging.
const VERBOSE = process.env.VERBOSE || 'false';

// The path to the WPT report file.
const WPT_REPORT = process.env.WPT_REPORT || 'wptreport.json';

// Only set WPT_METADATA if it's not already set.
let WPT_METADATA;
if (typeof process.env.WPT_METADATA === 'undefined') {
  if (CHROMEDRIVER === 'true') {
    WPT_METADATA = 'wpt-metadata/chromedriver/headless';
  } else {
    WPT_METADATA =
      HEADLESS === 'true'
        ? 'wpt-metadata/mapper/headless'
        : 'wpt-metadata/mapper/headful';
  }
} else {
  WPT_METADATA = process.env.WPT_METADATA;
}

if (HEADLESS === 'true') {
  log('Running WPT in headless mode...');
} else {
  log('Running WPT in headful mode...');
}

const wptRunArgs = [
  '--binary',
  BROWSER_BIN,
  '--webdriver-binary',
  'tools/run-bidi-server.mjs',
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
];

if (VERBOSE === 'true') {
  wptRunArgs.push(
    '--debug-test',
    '--log-mach',
    '-',
    '--log-mach-level',
    'info'
  );
} else if (process.env.CI === 'true' && HEADLESS === 'true') {
  // Parallelization is flaky in headful mode.
  wptRunArgs.push('--processes', cpus().length);
}

if (CHROMEDRIVER === 'true') {
  log('Using chromedriver with mapper...');
  wptRunArgs.push(
    '--binary-arg=--headless=new',
    '--install-webdriver',
    '--webdriver-arg=--bidi-mapper-path=lib/iife/mapperTab.js',
    '--webdriver-arg=--log-path=out/chromedriver.log',
    '--webdriver-arg=--verbose',
    '--yes'
  );
} else {
  log('Using pure mapper...');
}

const restArgs = process.argv.slice(2);
const test = restArgs[restArgs.length - 1] ?? 'webdriver/tests/bidi';

log(`Running "${test}" with "${BROWSER_BIN}"...`);

wptRunArgs.push(
  // All arguments except the first one (the command) and the last one (the test) are the flags.
  ...process.argv.slice(2, process.argv.length - 1),
  PRODUCT,
  // The last argument is the test.
  test
);

const {status} = spawnSync('./wpt/wpt', ['run', ...wptRunArgs], {
  stdio: 'inherit',
});

if (UPDATE_EXPECTATIONS === 'true') {
  log('Updating WPT expectations...');

  spawnSync(
    './wpt/wpt',
    [
      'update-expectations',
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
