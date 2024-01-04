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

import {spawn, spawnSync} from 'child_process';
import {createWriteStream, mkdirSync} from 'fs';
import {basename, join, resolve} from 'path';

import {packageDirectorySync} from 'pkg-dir';
import yargs from 'yargs';
import {hideBin} from 'yargs/helpers';

// Changing the current work directory to the package directory.
process.chdir(packageDirectorySync());

function log(message) {
  // eslint-disable-next-line no-console
  console.log(`(${basename(process.argv[1])}) ${message}`);
}

const argv = yargs(hideBin(process.argv))
  .usage(
    `[CHANNEL=<stable | beta | canary | dev>] [DEBUG=*] [DEBUG_COLORS=<yes | no>] [HEADLESS=<true | false>] [LOG_DIR=logs] [NODE_OPTIONS=--unhandled-rejections=strict] [PORT=8080] $0`
  )
  .option('headless', {
    describe:
      'Whether to start the server in headless or headful mode. The --headless flag takes precedence over the HEADLESS environment variable.',
    type: 'boolean',
    default: process.env.HEADLESS === 'true',
  })
  .parse();

let BROWSER_BIN = process.env.BROWSER_BIN;
let CHANNEL = process.env.CHANNEL || 'local';
if (CHANNEL === 'local') {
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  BROWSER_BIN = spawnSync('node', [join('tools', 'install-browser.mjs')])
    .stdout.toString()
    .trim();
  // Need to pass valid CHANNEL to the command below
  CHANNEL = 'canary';
}

// DEBUG = (empty string) is allowed.
const DEBUG = process.env.DEBUG ?? 'bidi:*';
const DEBUG_COLORS = process.env.DEBUG_COLORS || 'false';
const DEBUG_DEPTH = process.env.DEBUG_DEPTH || '10';
const LOG_DIR = process.env.LOG_DIR || 'logs';
const LOG_FILE =
  process.env.LOG_FILE ||
  join(LOG_DIR, `${new Date().toISOString().replace(/[:]/g, '-')}.log`);
const NODE_OPTIONS =
  process.env.NODE_OPTIONS || '--unhandled-rejections=strict';
const PORT = process.env.PORT || '8080';

log(`Starting BiDi Server with DEBUG='${DEBUG}'...`);

mkdirSync(LOG_DIR, {recursive: true});

const subprocess = spawn(
  'node',
  [
    resolve(join('lib', 'cjs', 'bidiServer', 'index.js')),
    `--channel`,
    CHANNEL,
    `--headless`,
    argv.headless,
    ...process.argv.slice(2),
  ],
  {
    stdio: ['inherit', 'pipe', 'pipe'],
    env: {
      ...process.env,
      // keep-sorted start
      BROWSER_BIN,
      DEBUG,
      DEBUG_COLORS,
      DEBUG_DEPTH,
      NODE_OPTIONS,
      PORT,
      // keep-sorted end
    },
  }
);

if (subprocess.stderr) {
  subprocess.stderr.pipe(process.stdout);
  subprocess.stderr.pipe(createWriteStream(LOG_FILE));
}

if (subprocess.stdout) {
  subprocess.stdout.pipe(process.stdout);
  subprocess.stdout.pipe(createWriteStream(LOG_FILE));
}

subprocess.on('exit', (status) => {
  process.exit(status || 0);
});
