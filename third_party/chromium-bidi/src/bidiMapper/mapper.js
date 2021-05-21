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

import { CdpServer } from './utils/cdpServer';
import { BidiServer } from './utils/bidiServer';
import { ServerBinding } from './utils/iServer';

import { log } from './utils/log';
const logSystem = log('system');

// `currentTargetId` is set by `setCurrentTargetId` + `Runtime.evaluate`.
let currentTargetId;

//`window.cdp` is exposed by `Target.exposeDevToolsProtocol`:
// https://chromedevtools.github.io/devtools-protocol/tot/Target/#method-exposeDevToolsProtocol
const cdpBinding = new ServerBinding(
  (message) => {
    window.cdp.send(message);
  },
  (handler) => {
    window.cdp.onmessage = handler;
  }
);
const cdpServer = new CdpServer(cdpBinding);

const bidiBinding = new ServerBinding(
  (message) => {
    // `window.sendBidiResponse` is exposed by `Runtime.addBinding` from the server side.
    window.sendBidiResponse(message);
  },
  (handler) => {
    // `window.onBidiMessage` is called via `Runtime.evaluate` from the server side.
    window.onBidiMessage = handler;
  }
);
const bidiServer = new BidiServer(bidiBinding);

// Needed to filter out info related to BiDi target.
window.setCurrentTargetId = function (targetId) {
  logSystem('current target ID: ' + targetId);
  currentTargetId = targetId;
};

const runBidiMapper = async function () {
  window.document.documentElement.innerHTML = `<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2>`;

  window.document.title = 'BiDi Mapper';

  await runBidiCommandsProcessor(cdpServer, bidiServer, () => currentTargetId);

  await cdpServer.sendMessage({
    method: 'Target.setDiscoverTargets',
    params: { discover: true },
  });

  logSystem('launched');
};

runBidiMapper();
