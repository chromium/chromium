/**
 * Copyright 2021 Google Inc. All rights reserved.
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
import { runBidiCommandsProcessor } from './bidiCommandsProcessor.js';
import { createBidiClient } from './bidiClient.js';

import { CdpBinding } from './utils/cdpServer';

import { log } from './utils/log';
const logSystem = log('system');

// `currentTargetId` is set by `setCurrentTargetId` + `Runtime.evaluate`.
let currentTargetId;

/**
 * `window.cdp` is exposed by `Target.exposeDevToolsProtocol`.
 */
const cdpServer = CdpBinding.runCdpServer(window.cdp.send, (handler) => {
  window.cdp.onmessage = handler;
});

// `window.sendBidiResponse` is exposed by `Runtime.addBinding`.
const sendBidiResponse = window.sendBidiResponse;
// Needed to filter out info related to BiDi target.
window.setCurrentTargetId = function (targetId) {
  logSystem('current target ID: ' + targetId);
  currentTargetId = targetId;
};

// Run via `Runtime.evaluate` from the bidi server side.
window.onBidiMessage = function (messageStr) {
  bidiClient.onBidiMessageReceived(messageStr);
};

const bidiClient = createBidiClient(sendBidiResponse);

const runBidiMapper = async function () {
  window.document.documentElement.innerHTML = `<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2>`;

  window.document.title = 'BiDi Mapper';

  await runBidiCommandsProcessor(cdpServer, bidiClient, () => currentTargetId);

  await cdpServer.sendMessage({
    method: 'Target.setDiscoverTargets',
    params: { discover: true },
  });

  logSystem('launched');
};

runBidiMapper();
