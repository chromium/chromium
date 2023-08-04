#!/usr/bin/env node
/* eslint-disable no-console */

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

/**
 * @fileoverview Installs a browser defined in `.browser` using
 * `@puppeteer/browsers` to the directory provided as the first argument
 * (default: cwd). The executable path is written to the `executablePath` output
 * param for GitHub actions.
 *
 * Examples:
 *  - `node tools/install-browser.mjs`
 *  - `node tools/install-browser.mjs /tmp/cache`
 */

import {readFile} from 'fs/promises';
import {homedir} from 'os';
import {resolve} from 'path';

import {install, computeExecutablePath} from '@puppeteer/browsers';
import actions from '@actions/core';

const SHELL_ARG = '--shell';

try {
  const browserSpec = (await readFile('.browser', 'utf-8')).trim();

  let cacheDir = resolve(homedir(), '.cache', 'chromium-bidi');
  if (process.argv[2] && process.argv[2] !== SHELL_ARG) {
    cacheDir = process.argv[2];
  }

  // See .browser for the format.
  const browser = browserSpec.split('@')[0];
  const buildId = browserSpec.split('@')[1];
  await install({
    browser,
    buildId,
    cacheDir,
  });
  const executablePath = computeExecutablePath({
    cacheDir,
    browser,
    buildId,
  });
  if (!process.argv.includes(SHELL_ARG)) {
    actions.setOutput('executablePath', executablePath);
  }
  console.log(executablePath);
} catch (err) {
  actions.setFailed(`Failed to download the browser: ${err.message}`);
}
