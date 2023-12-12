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
 * @fileoverview smoke test for WebdriverIO integration.
 *
 * Runs WebDriver bindings with the latest CfT + ChromeDriver + current Mapper version.
 */

import * as assert from 'node:assert';

import {remote} from 'webdriverio';

import {
  installAndGetChromePath,
  installAndGetChromeDriverPath,
  getBidiMapperPath,
} from '../tools/path-getter/path-getter.mjs';

const chromePath = installAndGetChromePath();
const chromeDriverPath = installAndGetChromeDriverPath();

let browser;

try {
  browser = await remote({
    capabilities: {
      browserName: 'chrome',
      'wdio:chromedriverOptions': {
        binary: chromeDriverPath,
        args: [`--bidi-mapper-path=${getBidiMapperPath()}`],
      },
      'goog:chromeOptions': {
        args: ['--enable-bidi', '--headless', '--disable-gpu'],
        binary: chromePath,
      },
      webSocketUrl: true,
    },
    logLevel: 'debug',
  });
  // Create a tab.
  const browsingContext = await browser.browsingContextCreate({
    type: 'tab',
  });

  // Navigate tab to some page.
  await browser.browsingContextNavigate({
    url: 'data:text/html,<h1>SOME PAGE</h1>',
    context: browsingContext.context,
    wait: 'complete',
  });

  // Get header element reference.
  const evaluateResult = await browser.scriptEvaluate({
    expression: 'document.getElementsByTagName("h1")[0]',
    target: {context: browsingContext.context},
    awaitPromise: false,
    resultOwnership: 'root',
  });
  assert.strictEqual(evaluateResult.type, 'success');
  const elementId = evaluateResult.result.sharedId;

  // Get screenshot of the element.
  const response = await browser.browsingContextCaptureScreenshot({
    context: browsingContext.context,
    clip: {
      type: 'element',
      element: {
        sharedId: elementId,
      },
    },
  });

  // Constants for checking the file format.
  const startIndex = 0;
  const endIndex = 5;
  const pngMagicNumber = 'iVBOR';

  const base64code = response.data.slice(startIndex, endIndex);
  assert.equal(base64code, pngMagicNumber);
} finally {
  if (browser) {
    await browser.deleteSession();
  }
}
