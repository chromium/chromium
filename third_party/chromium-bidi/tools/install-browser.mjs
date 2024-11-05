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

/**
 * @fileoverview Installs a browser defined in `.browser` or corresponding
 * ChromeDriver using `@puppeteer/browsers` to the directory provided as the
 * first argument (default: $HOME/.cache/chromium-bidi).
 *
 * If `--chromedriver` is set, the ChromeDriver is installed instead of a
 * browser.
 *
 * If `--chrome-headless-shell` is set, Headless Shell is installed.
 * https://developer.chrome.com/blog/chrome-headless-shell
 *
 * If `--github` is set, the executable path is written to the
 * `executablePath` output param for GitHub actions. Otherwise, the executable
 * is written to stdout.
 *
 * Examples:
 *  - `node tools/install-browser.mjs`
 *  - `node tools/install-browser.mjs /tmp/cache`
 *  - `node tools/install-browser.mjs --chromedriver`
 *  - `node tools/install-browser.mjs --chrome-headless-shell`
 */

import {readFile} from 'fs/promises';
import {homedir} from 'os';
import {resolve} from 'path';

import {setOutput, setFailed} from '@actions/core';
import {install, computeExecutablePath} from '@puppeteer/browsers';

const GITHUB_SHELL_ARG = '--github';
const CHROME_DRIVER_ARG = '--chromedriver';
const CHROME_HEADLESS_SHELL_ARG = '--chrome-headless-shell';

/**
 * Returns the browser name based on the command line arguments and `.browser`
 * content.
 * @return {'chrome'|'chromedriver'|'chrome-headless-shell'}
 */
function getProduct(browserSpec) {
  if (process.argv.includes(CHROME_DRIVER_ARG)) {
    return 'chromedriver';
  }
  if (process.argv.includes(CHROME_HEADLESS_SHELL_ARG)) {
    return 'chrome-headless-shell';
  }
  // Default `chrome`.
  return browserSpec.split('@')[0];
}

try {
  const browserSpec = (await readFile('.browser', 'utf-8')).trim();

  let cacheDir = resolve(homedir(), '.cache', 'chromium-bidi');
  if (
    process.argv[2] &&
    process.argv[2] !== GITHUB_SHELL_ARG &&
    process.argv[2] !== CHROME_DRIVER_ARG &&
    process.argv[2] !== CHROME_HEADLESS_SHELL_ARG
  ) {
    cacheDir = process.argv[2];
  }

  // See .browser for the format.
  const product = getProduct(browserSpec);
  const buildId = browserSpec.split('@')[1];

  await install({
    browser: product,
    buildId,
    cacheDir,
  });

  const executablePath = computeExecutablePath({
    cacheDir,
    browser: product,
    buildId,
  });
  if (process.argv.includes(GITHUB_SHELL_ARG)) {
    setOutput('executablePath', executablePath);
  }
  console.log(executablePath);
} catch (err) {
  setFailed(`Failed to download the browser: ${err.message}`);
}
