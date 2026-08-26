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

import {join} from 'path';

/**
 * Returns the browser binary path from BROWSER_BIN environment variable.
 * Throws if BROWSER_BIN is not set.
 * @return {string}
 */
export function getChromePath() {
  if (process.env.BROWSER_BIN) {
    return process.env.BROWSER_BIN;
  }
  throw new Error(
    'The BROWSER_BIN environment variable or --browser-bin argument must be provided.',
  );
}

/**
 * Returns the ChromeDriver binary path from CHROMEDRIVER_BIN environment variable.
 * Throws if CHROMEDRIVER_BIN is not set.
 * @return {string}
 */
export function getChromeDriverPath() {
  if (process.env.CHROMEDRIVER_BIN) {
    return process.env.CHROMEDRIVER_BIN;
  }
  throw new Error(
    'The CHROMEDRIVER_BIN environment variable or --chromedriver-bin argument must be provided.',
  );
}

/**
 * Returns the path to the mapperTab.js file.
 * @return {string}
 */
export function getBidiMapperPath() {
  return join('out', 'Default', 'gen', 'src', 'mapperTab.js');
}
