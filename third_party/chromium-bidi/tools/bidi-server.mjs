/**
 * Copyright 2024 Google LLC.
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
import child_process from 'child_process';
import {mkdirSync} from 'fs';
import {basename, join, resolve} from 'path';

import {parseArgs} from 'node:util';

import {
  getChromeDriverPath,
  getChromePath,
} from './path-getter/path-getter.mjs';

export function log(...message) {
  console.log(`(${basename(process.argv[1])})`, ...message);
}

const RUN_TIME = new Date().toISOString().replace(/[:]/g, '-');

export function getLogFileName(suffix) {
  const dir = process.env.LOG_DIR || 'logs';
  return resolve(
    process.env.LOG_FILE || join(dir, `${RUN_TIME}.${suffix}.log`),
  );
}

/**
 * @param {String} suffix
 */
export function createLogFile(suffix) {
  // Changing the current work directory to the package directory.
  process.chdir(join(import.meta.dirname, '..'));

  const dir = process.env.LOG_DIR || 'logs';
  const name = process.env.LOG_FILE || join(dir, `${RUN_TIME}.${suffix}.log`);

  mkdirSync(dir, {recursive: true});

  return resolve(name);
}

export function parseCommandLineArgs() {
  const args = process.argv.slice(2);
  let parsedArgs = args;
  while (parsedArgs.length > 0 && parsedArgs[0] === '--') {
    parsedArgs = parsedArgs.slice(1);
  }
  const {values, positionals} = parseArgs({
    args: parsedArgs,
    options: {
      k: {
        type: 'string',
      },
      s: {
        type: 'boolean',
      },
      'repeat-times': {
        type: 'string',
        default: String(process.env.REPEAT_TIMES || 1),
      },
      'reruns-times': {
        type: 'string',
        default: String(process.env.RERUNS_TIMES || 0),
      },
      'total-shards': {
        type: 'string',
        default: String(
          process.env.GTEST_TOTAL_SHARDS ||
            process.env.PYTEST_TOTAL_SHARDS ||
            1,
        ),
      },
      'shard-id': {
        type: 'string',
        default: String(
          process.env.GTEST_SHARD_INDEX || process.env.PYTEST_SHARD_ID || 0,
        ),
      },
      'gen-dir': {
        type: 'string',
      },
      'python-bin': {
        type: 'string',
      },
      'python-spec': {
        type: 'string',
      },
      'browser-bin': {
        type: 'string',
        default: process.env.BROWSER_BIN,
      },
      'chromedriver-bin': {
        type: 'string',
        default: process.env.CHROMEDRIVER_BIN,
      },
      'test-filter': {
        type: 'string',
      },
      'test-filter-file': {
        type: 'string',
      },
      'isolated-script-test-filter': {
        type: 'string',
      },
      'isolated-script-test-filter-file': {
        type: 'string',
      },
      gtest_filter: {
        type: 'string',
      },
      'gtest-filter': {
        type: 'string',
      },
      'isolated-script-test-output': {
        type: 'string',
      },
      'isolated-script-test-perf-output': {
        type: 'string',
      },
      'isolated-outdir': {
        type: 'string',
      },
      'isolated-script-test-repeat': {
        type: 'string',
      },
      gtest_repeat: {
        type: 'string',
      },
      'gtest-repeat': {
        type: 'string',
      },
      'isolated-script-test-launcher-retry-limit': {
        type: 'string',
      },
      'isolated-script-test-also-run-disabled-tests': {
        type: 'boolean',
      },
      shards: {
        type: 'string',
      },
    },
    allowPositionals: true,
    strict: false,
  });

  return {
    fileOrFolder: positionals,
    k: values.k,
    s: values.s,
    'repeat-times': Number(
      values['isolated-script-test-repeat'] ||
        values.gtest_repeat ||
        values['gtest-repeat'] ||
        values['repeat-times'] ||
        1,
    ),
    'reruns-times': Number(
      values['isolated-script-test-launcher-retry-limit'] ||
        values['reruns-times'] ||
        0,
    ),
    'total-shards': Number(values.shards || values['total-shards'] || 1),
    'shard-id': Number(values['shard-id'] || 0),
    'gen-dir': values['gen-dir'],
    'python-bin': values['python-bin'],
    'python-spec': values['python-spec'],
    'browser-bin': values['browser-bin'],
    'chromedriver-bin': values['chromedriver-bin'],
    'test-filter':
      values['test-filter'] ||
      values['isolated-script-test-filter'] ||
      values.gtest_filter ||
      values['gtest-filter'],
    'test-filter-file':
      values['test-filter-file'] || values['isolated-script-test-filter-file'],
    'isolated-script-test-output': values['isolated-script-test-output'],
  };
}

/**
 *
 * @returns {child_process.ChildProcessWithoutNullStreams}
 */
export function createBiDiServerProcess() {
  const argv = parseCommandLineArgs();
  if (argv['browser-bin']) {
    process.env.BROWSER_BIN = argv['browser-bin'];
  }
  if (argv['chromedriver-bin']) {
    process.env.CHROMEDRIVER_BIN = argv['chromedriver-bin'];
    process.env.CHROMEDRIVER = 'true';
  }
  const BROWSER_BIN = getChromePath();

  const CHROMEDRIVER = process.env.CHROMEDRIVER === 'true';

  const DEBUG = process.env.DEBUG ?? 'bidi:*';
  const DEBUG_COLORS = process.env.DEBUG_COLORS || 'false';
  const DEBUG_DEPTH = process.env.DEBUG_DEPTH || '10';
  const NODE_OPTIONS =
    process.env.NODE_OPTIONS ||
    '--unhandled-rejections=strict --trace-uncaught';
  const PORT = process.env.PORT || '8080';
  const VERBOSE = true;

  const GEN_DIR = argv['gen-dir'] || join('out', 'Default', 'gen');

  let runParams;
  if (CHROMEDRIVER) {
    runParams = {
      file: getChromeDriverPath(),
      args: [
        `--port=${PORT}`,
        `--bidi-mapper-path=${resolve(join(GEN_DIR, 'src', 'mapperTab.js'))}`,
        `--readable-timestamp`,
        ...(VERBOSE ? ['--verbose'] : []),
      ],
      options: {
        stdio: ['inherit', 'pipe', 'pipe'],
        env: {},
      },
    };
  } else {
    runParams = {
      file: process.execPath,
      args: [
        resolve(join(GEN_DIR, 'src', 'bidiServer', 'index.js')),
        ...process.argv.slice(2),
      ],
      options: {
        stdio: ['inherit', 'pipe', 'pipe'],
        env: {
          // keep-sorted start
          BROWSER_BIN,
          DEBUG,
          DEBUG_COLORS,
          DEBUG_DEPTH,
          NODE_DEBUG: DEBUG,
          NODE_OPTIONS,
          PORT,
          VERBOSE,
          // keep-sorted end
        },
      },
    };
  }

  log(
    `Starting ${CHROMEDRIVER ? 'ChromeDriver' : 'Mapper'} with DEBUG='${DEBUG}'...`,
  );

  if (process.env.VERBOSE === 'true' || process.env.CI) {
    log(`Environment variables:`, runParams.options);
    log(
      `Command: ${runParams.file} ${runParams.args.map((a) => (a.indexOf(' ') < 0 ? a : a.replaceAll(' ', '\\ '))).join(' ')}`,
    );
  }

  const options = {
    ...runParams.options,
    env: {
      ...process.env,
      ...runParams.options.env,
    },
  };

  return child_process.spawn(runParams.file, runParams.args, options);
}
