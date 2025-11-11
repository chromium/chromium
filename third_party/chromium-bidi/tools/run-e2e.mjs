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
import {Transform, PassThrough} from 'stream';

import {packageDirectorySync} from 'package-directory';

import {
  createBiDiServerProcess,
  parseCommandLineArgs,
  createLogFile,
  log,
  getLogFileName,
} from './bidi-server.mjs';
import {installAndGetChromePath} from './path-getter/path-getter.mjs';
// Changing the current work directory to the package directory.
process.chdir(packageDirectorySync());

const argv = parseCommandLineArgs();
const LOG_FILE = createLogFile('e2e');
const PYTEST_PREFIX = 'PyTest';

const PYTEST_TOTAL_CHUNKS = argv['total-chunks'];
const PYTEST_THIS_CHUNK = argv['this-chunk'];
const UPDATE_SNAPSHOT = argv['update-snapshot'] === 'true';
const REPEAT_TIMES = argv['repeat-times'];
const RERUNS_TIMES = argv['reruns-times'];

/**
 *
 * @param {import('child_process').ChildProcessWithoutNullStreams} subprocess
 * @returns
 */
async function matchLine(subprocess) {
  let resolver;
  let rejecter;
  const promise = new Promise((resolve, reject) => {
    resolver = resolve;
    rejecter = reject;
  });
  setTimeout(() => rejecter('Timeout after 10 sec'), 10_000);
  let stdout = '';

  function check() {
    for (const line of stdout.split(/\n/g)) {
      if (/.*(BiDi server|ChromeDriver) was started successfully/.test(line)) {
        subprocess.off('exit', onExit);
        subprocess.stdout.off('data', onStdout);
        subprocess.stderr.off('data', onStdout);

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
    subprocess.off('exit', onExit);
    subprocess.stdout.off('data', onStdout);
    subprocess.stderr.off('data', onStdout);
    rejecter(stdout);
  }

  subprocess.stdout.on('data', onStdout);
  subprocess.stderr.on('data', onStdout);
  subprocess.on('exit', onExit);

  return await promise;
}

const addPrefix = () =>
  new Transform({
    transform(chunk, _, callback) {
      let line = String(chunk);
      if (line.startsWith('\n')) {
        line = line.replace('\n', '');
      }
      this.push(Buffer.from([`\n${PYTEST_PREFIX}: `, line, '\n'].join('')));
      callback(null);
    },
  });

class SyncFileStreams extends Transform {
  // Matches lines like `PyTest: XXX [ 39%]` or
  static percentRegEx = /\[ {0,2}\d{1,3}%\]/;
  serverLogs = Buffer.from('');

  _transform(chunk, _, callback) {
    let line = String(chunk);
    // Handles output from PyTest
    if (line.includes(PYTEST_PREFIX)) {
      if (SyncFileStreams.percentRegEx.test(line)) {
        if (line.includes('FAILED') || line.includes('ERROR')) {
          this.push(this.serverLogs);
        }
        this.serverLogs = Buffer.from('');
      }
      this.push(chunk);
      // Handles output from BiDi server
    } else {
      this.serverLogs = Buffer.concat([this.serverLogs, chunk]);
    }

    callback(null);
  }
}

const fileWriteStream = createWriteStream(LOG_FILE);
const syncFileStreams =
  process.env.VERBOSE === 'true' ? new PassThrough() : new SyncFileStreams();
syncFileStreams.pipe(fileWriteStream);

const serverProcess = createBiDiServerProcess();

if (serverProcess.stderr) {
  serverProcess.stderr.pipe(syncFileStreams);
}

if (serverProcess.stdout) {
  serverProcess.stdout.pipe(syncFileStreams);
  serverProcess.stdout.pipe(process.stdout);
}

await matchLine(serverProcess).catch((error) => {
  serverProcess.kill();
  log('Could not match line exiting...');
  log(error);
  process.exit(1);
});

const e2eArgs = ['run', 'pytest'];
e2eArgs.push(
  '--verbose',
  '-vv',
  // Do not throw an error if there are unused snapshots.
  '--snapshot-warn-unused',
  ...(UPDATE_SNAPSHOT ? ['--snapshot-update'] : []),
);
if (REPEAT_TIMES !== 1) {
  e2eArgs.push(`--count=${REPEAT_TIMES}`);
}
if (RERUNS_TIMES !== 0) {
  e2eArgs.push(`--reruns=${RERUNS_TIMES}`);
}
if (PYTEST_TOTAL_CHUNKS !== 1) {
  e2eArgs.push(
    '--num-shards',
    PYTEST_TOTAL_CHUNKS,
    '--shard-id',
    PYTEST_THIS_CHUNK,
  );
}

if (argv.fileOrFolder) {
  e2eArgs.push(
    ...(Array.isArray(argv.fileOrFolder)
      ? argv.fileOrFolder
      : [argv.fileOrFolder]),
  );
}

// Default headless is `new`.
const HEADLESS = ['old', 'new', 'false'].includes(process.env.HEADLESS)
  ? process.env.HEADLESS
  : 'new';

if (HEADLESS === 'false' && !argv.k) {
  // Ignore headful input e2e tests, as they are too slow.
  e2eArgs.push('--ignore=tests/input');
}
if (argv.k) {
  e2eArgs.push('-k', argv.k);
}
if (argv.s) {
  e2eArgs.push('-s');
}

const e2eProcess = child_process.spawn('pipenv', e2eArgs, {
  stdio: ['inherit', 'pipe', 'pipe'],
  env: {
    ...process.env,
    BROWSER_BIN: installAndGetChromePath(HEADLESS === 'old'),
    HEADLESS,
  },
});

if (e2eProcess.stderr) {
  e2eProcess.stderr.pipe(process.stdout);
  e2eProcess.stderr.pipe(addPrefix()).pipe(syncFileStreams);
}

if (e2eProcess.stdout) {
  e2eProcess.stdout.pipe(process.stdout);
  e2eProcess.stdout.pipe(addPrefix()).pipe(syncFileStreams);
}

e2eProcess.on('error', (error) => {
  console.log('starting e2e tests failed', error);
  serverProcess.kill();
  process.exit(1);
});

e2eProcess.on('exit', (status) => {
  serverProcess.kill();

  if (status !== 0) {
    log('\n\n', `Logs for the run can be found at ${getLogFileName('e2e')}`);
  }

  process.exit(status ?? 0);
});
