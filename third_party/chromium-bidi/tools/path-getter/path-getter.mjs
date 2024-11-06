/*
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

import {execSync} from 'child_process';
import {join} from 'path';

import {Browser, computeSystemExecutablePath} from '@puppeteer/browsers';

/**
 * Either return a value of `BROWSER_BIN` environment variable or gets the path
 * to the browser specified in `CHANNEL` environment variable if specified. If
 * no channel or browser bin is provided in environment variable,
 * returns defined in `.browser` file, downloading the required version if
 * needed. If `HEADLESS` environment variable is set to `old`, downloads the
 * Headless Shell instead of the Chrome binary.
 * @return {string}
 */
export function installAndGetChromePath(isHeadlessShell = false) {
  // Old headless means "headless shell", which is implemented in a separate
  // binary: https://developer.chrome.com/blog/chrome-headless-shell.
  isHeadlessShell = isHeadlessShell || process.env.HEADLESS === 'old';

  if (process.env.BROWSER_BIN) {
    return process.env.BROWSER_BIN;
  }

  const channel = getChannel();

  if (channel === 'local') {
    const commandArray = ['node', join('tools', 'install-browser.mjs')];
    if (isHeadlessShell) {
      commandArray.push('--chrome-headless-shell');
    }
    return execSync(commandArray.join(' ')).toString().trim();
  }
  if (isHeadlessShell) {
    throw new Error(
      'Auto download of headless shell is supported only for ' +
        '`local` channel. Either use `CHANNEL=local` or set `BROWSER_BIN` ' +
        'environment variable to the path of the headless shell binary.',
    );
  }

  return computeSystemExecutablePath({
    browser: Browser.CHROME,
    channel,
  });
}

/**
 * Either return a value of `CHROMEDRIVER_BIN` environment variable or gets the
 * path to the ChromeDriver for the browser version defined in `.browser` file,
 * downloading the required version if needed. Throws an error if the channel is
 * not `local` and the `CHROMEDRIVER_BIN` environment variable is not set.
 * @return {string}
 */
export function installAndGetChromeDriverPath() {
  if (process.env.CHROMEDRIVER_BIN) {
    return process.env.CHROMEDRIVER_BIN;
  }

  if (getChannel() !== 'local') {
    throw new Error(
      'Auto download of chromedriver is supported only for `local` channel. Either use `CHANNEL=local` or set `CHROMEDRIVER_BIN` environment variable to the path of the chromedriver binary matching required Chrome channel.',
    );
  }

  return execSync(
    ['node', join('tools', 'install-browser.mjs'), '--chromedriver'].join(' '),
  )
    .toString()
    .trim();
}

/**
 * Returns the path to the mapperTab.js file.
 * @return {string}
 */
export function getBidiMapperPath() {
  return join('lib', 'iife', 'mapperTab.js');
}

function getChannel() {
  return process.env.CHANNEL || 'local';
}
