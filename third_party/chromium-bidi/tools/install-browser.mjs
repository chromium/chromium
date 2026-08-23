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
 * @fileoverview Installs Chrome binaries, ChromeDriver, Headless Shell or all
 * of them with version defined in `.browser` using `@puppeteer/browsers` to the
 * directory provided as the first argument or to the default of
 * $HOME/.cache/chromium-bidi.
 *
 * If `--chrome` is set, or no other flags is set, Chrome is installed.
 *
 * If `--chromedriver` is set, the ChromeDriver is installed.
 *
 * If `--chrome-headless-shell` is set, Headless Shell is installed.
 * https://developer.chrome.com/blog/chrome-headless-shell
 *
 * If `--all` is set, Chrome, Chromedriver and Headless Shell is installed.
 * Same as using `--chrome --chromedriver --chrome-headless-shell`.
 *
 * If `--github` is set, the executable path is written to the
 * `executablePath` output param for GitHub actions. Otherwise, the executable
 * is written to stdout. Can be used with at most one product argument.
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
const ALL_ARG = '--all';
const CHROME_ARG = '--chrome';
const CHROME_DRIVER_ARG = '--chromedriver';
const CHROME_HEADLESS_SHELL_ARG = '--chrome-headless-shell';
const ARGUMENTS = [
  ALL_ARG,
  CHROME_ARG,
  CHROME_DRIVER_ARG,
  CHROME_HEADLESS_SHELL_ARG,
  GITHUB_SHELL_ARG,
];

/**
 * Returns the browser name based on the command line arguments and `.browser`
 * content.
 * @return {['chrome'|'chromedriver'|'chrome-headless-shell']}
 */
function getProducts() {
  const result = [];
  if (
    process.argv.includes(CHROME_DRIVER_ARG) ||
    process.argv.includes(ALL_ARG)
  ) {
    result.push('chromedriver');
  }
  if (
    process.argv.includes(CHROME_HEADLESS_SHELL_ARG) ||
    process.argv.includes(ALL_ARG)
  ) {
    result.push('chrome-headless-shell');
  }
  if (
    process.argv.includes(CHROME_ARG) ||
    process.argv.includes(ALL_ARG) ||
    result.length === 0
  ) {
    result.push('chrome');
  }
  return result;
}

try {
  const browserSpec = (await readFile('.browser', 'utf-8')).trim();

  let cacheDir = resolve(homedir(), '.cache', 'chromium-bidi');
  if (process.argv[2] && !ARGUMENTS.includes(process.argv[2])) {
    cacheDir = process.argv[2];
  }

  // See .browser for the format.
  const products = getProducts();
  const buildId = browserSpec.split('@')[1];

  // Install all the requested binaries.
  const paths = await Promise.all(
    products.map(async (product) => {
      await install({
        browser: product,
        buildId,
        cacheDir,
      });
      return computeExecutablePath({
        cacheDir,
        browser: product,
        buildId,
      });
    }),
  );

  console.log(paths.join('\n'));
  if (process.argv.includes(GITHUB_SHELL_ARG)) {
    if (paths.length === 1) {
      setOutput('executablePath', paths[0]);
    } else {
      setFailed(
        `${GITHUB_SHELL_ARG} flag cannot be used for more then one product`,
      );
    }
  }
} catch (err) {
  setFailed(`Failed to download the browser: ${err.message}`);
}
