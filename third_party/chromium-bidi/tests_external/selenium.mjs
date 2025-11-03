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

/**
 * @fileoverview smoke test for Selenium integration.
 *
 * Runs Selenium with the latest CfT + ChromeDriver + current Mapper version.
 * Inspired by https://github.com/SeleniumHQ/selenium/blob/0c86525184355bddc44b6193ae7236f11a7fb129/javascript/node/selenium-webdriver/test/bidi/bidi_test.js#L300
 */

import * as assert from 'node:assert';

import {Builder, ScriptManager, BrowsingContext} from 'selenium-webdriver';
import chrome from 'selenium-webdriver/chrome.js';

import {
  installAndGetChromePath,
  installAndGetChromeDriverPath,
  getBidiMapperPath,
} from '../tools/path-getter/path-getter.mjs';

const chromePath = installAndGetChromePath();
const chromeDriverPath = installAndGetChromeDriverPath();

const chromeService = new chrome.ServiceBuilder(chromeDriverPath).addArguments(
  `--bidi-mapper-path=${getBidiMapperPath()}`,
);

const driver = new Builder()
  .forBrowser('chrome')
  .setChromeOptions(
    new chrome.Options()
      .enableBidi()
      .addArguments('--disable-gpu')
      .setChromeBinaryPath(chromePath),
  )
  .setChromeService(chromeService)
  .build();

try {
  // Create a tab.
  const browsingContext = await BrowsingContext(driver, {
    type: 'tab',
  });

  // Navigate tab to some page.
  await browsingContext.navigate(
    'data:text/html,<h1>SOME PAGE</h1>',
    'complete',
  );

  const scriptManager = await ScriptManager(browsingContext, driver);

  // Get header element reference.
  const evaluateResult = await scriptManager.evaluateFunctionInBrowsingContext(
    browsingContext.id,
    '(document.getElementsByTagName("h1")[0])',
    false,
    'root',
  );
  assert.strictEqual(evaluateResult.resultType, 'success');
  const elementId = evaluateResult.result.sharedId;

  // Get screenshot of the element.
  const response = await browsingContext.captureElementScreenshot(elementId);

  // Constants for checking the file format.
  const startIndex = 0;
  const endIndex = 5;
  const pngMagicNumber = 'iVBOR';

  const base64code = response.slice(startIndex, endIndex);
  assert.equal(base64code, pngMagicNumber);
} finally {
  await driver.quit();
}
