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

/**
 * Either return a value of `BROWSER_BIN` environment variable or gets the path
 * to the browser version defined in `.browser` file, downloading the required
 * version if needed.
 * @return {string}
 */
export function installAndGetChromePath() {
  let BROWSER_BIN = process.env.BROWSER_BIN;
  if (!BROWSER_BIN) {
    BROWSER_BIN = execSync(
      ['node', join('tools', 'install-browser.mjs')].join(' ')
    )
      .toString()
      .trim();
  }
  return BROWSER_BIN;
}

/**
 * Either return a value of `CHROMEDRIVER_BIN` environment variable or gets the
 * path to the ChromeDriver for the browser version defined in `.browser` file,
 * downloading the required version if needed.
 * @return {string}
 */
export function installAndGetChromeDriverPath() {
  let CHROMEDRIVER_BIN = process.env.CHROMEDRIVER_BIN;
  if (!CHROMEDRIVER_BIN) {
    CHROMEDRIVER_BIN = execSync(
      ['node', join('tools', 'install-browser.mjs'), '--chromedriver'].join(' ')
    )
      .toString()
      .trim();
  }
  return CHROMEDRIVER_BIN;
}

/**
 * Returns the path to the mapperTab.js file.
 * @return {string}
 */
export function getBidiMapperPath() {
  return join('lib', 'iife', 'mapperTab.js');
}
