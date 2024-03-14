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

import {packageDirectorySync} from 'pkg-dir';
import yargs from 'yargs';
import {hideBin} from 'yargs/helpers';

function log(message) {
  // eslint-disable-next-line no-console
  console.log(`(${basename(process.argv[1])}) ${message}`);
}

/**
 * @param {String} suffix
 */
export function createLogFile(suffix) {
  // Changing the current work directory to the package directory.
  process.chdir(packageDirectorySync());

  const LOG_DIR = process.env.LOG_DIR || 'logs';
  const LOG_FILE =
    process.env.LOG_FILE ||
    join(
      LOG_DIR,
      `${new Date().toISOString().replace(/[:]/g, '-')}.${suffix}.log`
    );

  mkdirSync(LOG_DIR, {recursive: true});

  return LOG_FILE;
}

export function parseCommandLineArgs() {
  return yargs(hideBin(process.argv))
    .usage(
      `$0 [fileOrFolder]`,
      `[CHANNEL=<stable | beta | canary | dev>] [DEBUG=*] [DEBUG_COLORS=<yes | no>] [LOG_DIR=logs] [NODE_OPTIONS=--unhandled-rejections=strict] [PORT=8080]`,
      (yargs) => {
        yargs.positional('fileOrFolder', {
          describe: 'Provide a sub E2E file or folder to filter by',
          type: 'string',
        });
      }
    )
    .option('k', {
      describe: 'Provide a test name to filter by',
      type: 'string',
    })
    .parse();
}

/**
 *
 * @returns {child_process.ChildProcessWithoutNullStreams}
 */
export function createBiDiServerProcess() {
  let BROWSER_BIN = process.env.BROWSER_BIN;
  let CHANNEL = process.env.CHANNEL || 'local';

  if (BROWSER_BIN) {
    // Need to pass valid CHANNEL to the command below
    CHANNEL = CHANNEL === 'local' ? 'canary' : CHANNEL;
  }

  if (!BROWSER_BIN && CHANNEL === 'local') {
    // eslint-disable-next-line @typescript-eslint/no-unused-vars
    BROWSER_BIN = child_process
      .spawnSync('node', [join('tools', 'install-browser.mjs')])
      .stdout.toString()
      .trim();
    // Need to pass valid CHANNEL to the command below
    CHANNEL = 'canary';
  }
  // DEBUG = (empty string) is allowed.
  const DEBUG = process.env.DEBUG ?? 'bidi:*';
  const DEBUG_COLORS = process.env.DEBUG_COLORS || 'false';
  const DEBUG_DEPTH = process.env.DEBUG_DEPTH || '10';
  const NODE_OPTIONS =
    process.env.NODE_OPTIONS ||
    '--unhandled-rejections=strict --trace-uncaught';
  const PORT = process.env.PORT || '8080';
  log(`Starting BiDi Server with DEBUG='${DEBUG}'...`);

  return child_process.spawn(
    'node',
    [
      resolve(join('lib', 'cjs', 'bidiServer', 'index.js')),
      `--channel`,
      CHANNEL,
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
        VERBOSE: true,
        // keep-sorted end
      },
    }
  );
}
