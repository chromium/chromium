#!/usr/bin/env node

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
import {createWriteStream} from 'fs';
import {Transform} from 'stream';

import {packageDirectorySync} from 'pkg-dir';

import {
  createBiDiServerProcess,
  parseCommandLineArgs,
  createLogFile,
} from './bidi-server.mjs';
// Changing the current work directory to the package directory.
process.chdir(packageDirectorySync());

const argv = parseCommandLineArgs();
const LOG_FILE = createLogFile('e2e');

/**
 *
 * @param {import('child_process').ChildProcessWithoutNullStreams} process
 * @returns
 */
async function matchLine(process) {
  let resolver;
  let rejecter;
  const promise = new Promise((resolve, reject) => {
    resolver = resolve;
    rejecter = reject;
  });
  let stdout = '';
  function check() {
    for (const line of stdout.split(/\n/g)) {
      if (/.*BiDi server is listening on port \d+/.test(line)) {
        process.off('exit', onExit);
        process.stdout.off('data', onStdout);
        process.stderr.off('data', onStdout);

        resolver();
        break;
      }
    }
  }

  function onStdout(data) {
    stdout = stdout + String(data);
    check();
  }
  function onExit() {
    process.off('exit', onExit);
    process.stdout.off('data', onStdout);
    process.stderr.off('data', onStdout);
    rejecter();
  }
  process.stdout.on('data', onStdout);
  process.stderr.on('data', onStdout);
  process.on('exit', onExit);

  return await promise;
}

const addPrefix = () =>
  new Transform({
    transform(chunk, _, callback) {
      let line = String(chunk);
      if (line.startsWith('\n')) {
        line = line.replace('\n', '');
      }
      this.push(Buffer.from([`\nPyTest: `, line, '\n'].join('')));
      callback(null);
    },
  });

const fileWriteStream = createWriteStream(LOG_FILE);
const serverProcess = createBiDiServerProcess(argv);

if (serverProcess.stderr) {
  serverProcess.stdout.pipe(process.stdout);
  serverProcess.stderr.pipe(fileWriteStream);
}

if (serverProcess.stdout) {
  serverProcess.stdout.pipe(fileWriteStream);
}

await matchLine(serverProcess).catch(() => {
  // eslint-disable-next-line no-console
  console.log('Could not match line exiting...');
  process.exit(1);
});

const e2eArgs = ['run', 'pytest', '--verbose', '-vv'];
if (!argv.headless) {
  e2eArgs.push('--ignore=tests/input');
}
const e2eProcess = child_process.spawn('pipenv', e2eArgs, {
  stdio: ['inherit', 'pipe', 'pipe'],
});

if (e2eProcess.stderr) {
  e2eProcess.stderr.pipe(process.stdout);
  e2eProcess.stderr.pipe(addPrefix()).pipe(fileWriteStream);
}

if (e2eProcess.stdout) {
  e2eProcess.stdout.pipe(process.stdout);
  e2eProcess.stdout.pipe(addPrefix()).pipe(fileWriteStream);
}

e2eProcess.on('exit', (status) => {
  serverProcess.kill();
  process.exit(status ?? 0);
});
