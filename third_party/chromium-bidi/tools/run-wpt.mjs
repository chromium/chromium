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

import {spawnSync, spawn} from 'child_process';
import {mkdirSync, existsSync} from 'fs';
import {join, resolve} from 'path';

import {packageDirectorySync} from 'pkg-dir';

import {
  installAndGetChromeDriverPath,
  installAndGetChromePath,
} from './path-getter/path-getter.mjs';

// Changing the current work directory to the package directory.
process.chdir(packageDirectorySync());

function log(message, ...messages) {
  // eslint-disable-next-line no-console
  console.log(`(${process.argv[1]}) ${message}`, ...messages);
}

function usage() {
  log(
    `Usage:
      [BROWSER_BIN=<path, default: download pinned chrome>]
      [CHROMEDRIVER=<true | default: false>]
      [HEADLESS=<default: true | false>]
      [MANIFEST=<default: 'MANIFEST.json'>]
      [RUN_TESTS=<default: true | false>]
      [TIMEOUT_MULTIPLIER=<number, default: 1>]
      [THIS_CHUNK=<number, default: 1>]
      [TOTAL_CHUNKS=<number, default: 1>]
      [UPDATE_EXPECTATIONS=<true | default: false>]
      [VERBOSE==<true | default: false>]
      [WPT_REPORT=<default: 'wptreport.json'>]
      [WPT_METADATA=<default: 'wpt-metadata/$\{ 'chromedriver' | 'mapper' }/$\{ 'headless' | 'headful' }>]
      ${process.argv[1]}
      [--wpt-report WPT_REPORT]
      [webdriver/tests/bidi/[...]]`
  );
}

if (
  process.argv.length > 2 &&
  (process.argv.includes('-h') || process.argv.includes('--help'))
) {
  usage();
  process.exit(0);
}
const restArgs = process.argv.slice(2);

// Weather to run the tests. `false` value is useful for updating expectations
// based on the WPT report.
const RUN_TESTS = process.env.RUN_TESTS || 'true';

// Whether to update the WPT expectations after running the tests.
const UPDATE_EXPECTATIONS = process.env.UPDATE_EXPECTATIONS || 'false';

let BROWSER_BIN = process.env.BROWSER_BIN;
if (RUN_TESTS === 'true' && !BROWSER_BIN) {
  BROWSER_BIN = installAndGetChromePath();
}

// Whether to use Chromedriver with mapper.
const CHROMEDRIVER = process.env.CHROMEDRIVER || 'true';

let CHROMEDRIVER_BIN = process.env.CHROMEDRIVER_BIN;
if (RUN_TESTS === 'true' && !CHROMEDRIVER_BIN) {
  CHROMEDRIVER_BIN =
    CHROMEDRIVER === 'true'
      ? installAndGetChromeDriverPath()
      : join('tools', 'run-bidi-server.mjs');
}

// Whether to fail when 0 test are run or not
const FAIL_NO_TEST = process.env.FAIL_NO_TEST || 'true';

// Whether to start the server in headless or headful mode.
const HEADLESS = process.env.HEADLESS || 'true';

// A special name for the logs to remove overriding no
// Don't provide extension`.log`
const CHROMEDRIVER_LOG_NAME =
  process.env.CHROMEDRIVER_LOG_NAME || 'chromedriver';

// The path to the WPT manifest file.
const MANIFEST = process.env.MANIFEST || 'MANIFEST.json';

// The browser product to test.
const PRODUCT = process.env.PRODUCT || 'chrome';

// Multiplier relative to standard test timeout to use.
const TIMEOUT_MULTIPLIER = process.env.TIMEOUT_MULTIPLIER || '1';

// The current chunk number. Required for shard testing.
const THIS_CHUNK = process.env.THIS_CHUNK || '1';

// The total number of chunks. Required for shard testing.
const TOTAL_CHUNKS = process.env.TOTAL_CHUNKS || '1';

// Whether to enable verbose logging.
const VERBOSE = process.env.VERBOSE || 'false';

// The path to the WPT report file.
let WPT_REPORT = process.env.WPT_REPORT || 'wptreport.json';

// If provided a CLI `--metadata` parameter, use it.
if (restArgs.includes('--wpt-report')) {
  WPT_REPORT = restArgs[restArgs.indexOf('--wpt-report') + 1];
}

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

const wptBinary = resolve(join('wpt', 'wpt'));

let runResult = undefined;
let updateResult = undefined;

if (RUN_TESTS === 'true') {
  const wptRunArgs = [
    '--binary',
    BROWSER_BIN,
    '--webdriver-binary',
    CHROMEDRIVER_BIN,
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
    '--total-chunks',
    TOTAL_CHUNKS,
    '--this-chunk',
    THIS_CHUNK,
    '--chunk-type',
    'hash',
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

  if (HEADLESS === 'true') {
    if (CHROMEDRIVER === 'true') {
      // For chromedriver use new headless.
      wptRunArgs.push('--binary-arg=--headless=new');
    } else {
      // TODO: switch to new headless.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/949.
      // For nodejs mapper runner supports only old headless.
      wptRunArgs.push('--binary-arg=--headless=old');
      wptRunArgs.push('--binary-arg=--hide-scrollbars');
      wptRunArgs.push('--binary-arg=--mute-audio');
    }
  } else {
    // Pass `--no-headless` to the WPT runner to enable headful mode.
    wptRunArgs.push('--no-headless');
  }

  if (CHROMEDRIVER === 'true') {
    if (!existsSync(join('logs'))) {
      mkdirSync(join('logs'));
    }

    const headlessSuffix = HEADLESS === 'true' ? '-headless' : '';
    const timestamp = `-${new Date().toISOString().replace(/[:]/g, '-')}`;
    const chromeDriverLogName = `${CHROMEDRIVER_LOG_NAME}${headlessSuffix}${timestamp}.log`;
    const chromeDriverLogs = join('logs', chromeDriverLogName);

    log('Using chromedriver with mapper...');
    wptRunArgs.push(
      `--webdriver-arg=--bidi-mapper-path=${join(
        'lib',
        'iife',
        'mapperTab.js'
      )}`,
      `--webdriver-arg=--log-path=${chromeDriverLogs}`,
      `--webdriver-arg=--log-level=${VERBOSE === 'true' ? 'ALL' : 'INFO'}`
    );
  } else {
    log('Using pure mapper...');
    wptRunArgs.push('--webdriver-binary', join('tools', 'run-bidi-server.mjs'));
  }

  let test =
    restArgs[restArgs.length - 1] ?? join('webdriver', 'tests', 'bidi');

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

  // TODO: escaping here is not quite correct.
  log(`${wptBinary} run ${wptRunArgs.map((arg) => `'${arg}'`).join(' ')}`);
  let resolver;
  let output = '';
  const promise = new Promise((resolve) => (resolver = resolve));
  const wptRun = spawn(wptBinary, ['run', ...wptRunArgs], {
    stdio: 'pipe',
  });

  wptRun.stdout.setEncoding('utf8');
  wptRun.stdout.pipe(process.stdout);
  wptRun.stdout.on('data', (data) => {
    output += data.toString();
  });

  wptRun.stderr.setEncoding('utf8');
  wptRun.stderr.pipe(process.stderr);
  wptRun.stderr.on('data', (data) => {
    output += data.toString();
  });

  wptRun.on('error', () => {
    runResult = {status: 1, output};
    resolver();
  });

  wptRun.on('exit', (status) => {
    runResult = {status, output};
    resolver();
  });

  await promise;
}

if (
  (UPDATE_EXPECTATIONS === 'true' && RUN_TESTS !== 'true') ||
  (UPDATE_EXPECTATIONS === 'true' && RUN_TESTS === 'true' && runResult)
) {
  log('Updating WPT expectations...');

  const wptRunArgs = [
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
  ];
  log(`${wptBinary} ${wptRunArgs.map((arg) => `'${arg}'`).join(' ')}`);

  updateResult = spawnSync(wptBinary, wptRunArgs, {stdio: 'inherit'});
}

// If WPT tests themselves or the expectations update failed, return failure.
let exitCode = 0;
if ((runResult?.status ?? 0) !== 0) {
  if (
    !(
      FAIL_NO_TEST === 'false' &&
      runResult.output
        .toString()
        .includes('Unable to find any tests at the path')
    )
  ) {
    log('WPT test run failed');
    exitCode = runResult.status;
  }
}
if ((updateResult?.status ?? 0) !== 0) {
  log('Update expectations failed');
  exitCode = updateResult.status;
}

process.exit(exitCode);
