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
  installAndGetChromeDriverPath,
  installAndGetChromePath,
} from './path-getter/path-getter.mjs';

export function log(...message) {
  console.log(`(${basename(process.argv[1])})`, ...message);
}

const RUN_TIME = new Date().toISOString().replace(/[:]/g, '-');

export function getLogFileName(suffix) {
  const dir = process.env.LOG_DIR || 'logs';
  return process.env.LOG_FILE || join(dir, `${RUN_TIME}.${suffix}.log`);
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

  return name;
}

export function parseCommandLineArgs() {
  const {values, positionals} = parseArgs({
    args: process.argv.slice(2),
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
      'total-chunks': {
        type: 'string',
        default: String(process.env.PYTEST_TOTAL_CHUNKS || 1),
      },
      'this-chunk': {
        type: 'string',
        default: String(process.env.PYTEST_THIS_CHUNK || 0),
      },
    },
    allowPositionals: true,
  });

  return {
    fileOrFolder: positionals,
    k: values.k,
    s: values.s,
    'repeat-times': Number(values['repeat-times']),
    'reruns-times': Number(values['reruns-times']),
    'total-chunks': Number(values['total-chunks']),
    'this-chunk': Number(values['this-chunk']),
  };
}

/**
 *
 * @returns {child_process.ChildProcessWithoutNullStreams}
 */
export function createBiDiServerProcess() {
  const BROWSER_BIN = installAndGetChromePath();

  const CHROMEDRIVER = process.env.CHROMEDRIVER === 'true';

  const DEBUG = process.env.DEBUG ?? 'bidi:*';
  const DEBUG_COLORS = process.env.DEBUG_COLORS || 'false';
  const DEBUG_DEPTH = process.env.DEBUG_DEPTH || '10';
  const NODE_OPTIONS =
    process.env.NODE_OPTIONS ||
    '--unhandled-rejections=strict --trace-uncaught';
  const PORT = process.env.PORT || '8080';
  const VERBOSE = true;

  let runParams;
  if (CHROMEDRIVER) {
    runParams = {
      file: installAndGetChromeDriverPath(),
      args: [
        `--port=${PORT}`,
        `--bidi-mapper-path=${resolve(join('out', 'Default', 'lib', 'iife', 'mapperTab.js'))}`,
        `--log-path=${createLogFile('chromedriver')}`,
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
      file: 'node',
      args: [
        resolve(join('out', 'Default', 'gen', 'src', 'bidiServer', 'index.js')),
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

  const options = CHROMEDRIVER
    ? runParams.options
    : {
        ...runParams.options,
        env: {
          ...process.env,
          ...runParams.options.env,
        },
      };

  return child_process.spawn(runParams.file, runParams.args, options);
}
