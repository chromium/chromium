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
import { CommandProcessor } from './commandProcessor';

import { CdpServer } from './utils/cdpServer';
import { BidiServer } from './utils/bidiServer';
import { ServerBinding } from './utils/iServer';

import { log } from './utils/log';
const logSystem = log('system');

declare global {
  interface Window extends GlobalObj {}

  interface GlobalObj {
    //`window.cdp` is exposed by `Target.exposeDevToolsProtocol` from the server side.
    // https://chromedevtools.github.io/devtools-protocol/tot/Target/#method-exposeDevToolsProtocol
    cdp: {
      send: (string) => void;
      onmessage: (string) => void;
    };

    // `window.sendBidiResponse` is exposed by `Runtime.addBinding` from the server side.
    sendBidiResponse: (string) => void;

    // `window.onBidiMessage` is called via `Runtime.evaluate` from the server side.
    onBidiMessage: (string) => void;

    // `window.setSelfTargetId` is called via `Runtime.evaluate` from the server side.
    setSelfTargetId: (string) => void;
  }
}

const globalObj = window as GlobalObj;

// Initiate `setSelfTargetId` as soon as possible to prevent race condition.
const _waitSelfTargetIdPromise = _waitSelfTargetId();

(async () => {
  window.document.documentElement.innerHTML = `<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2>`;
  window.document.title = 'BiDi Mapper';

  const cdpServer = _createCdpServer();
  const bidiServer = _createBidiServer();

  // Needed to filter out info related to BiDi target.
  const selfTargetId = await _waitSelfTargetIdPromise;

  // Needed to get events about new targets.
  await _prepareCdp(cdpServer);

  CommandProcessor.run(cdpServer, bidiServer, selfTargetId);

  logSystem('launched');

  bidiServer.sendMessage('launched');
})();

function _createCdpServer() {
  const cdpBinding = new ServerBinding(
    (message) => {
      globalObj.cdp.send(message);
    },
    (handler) => {
      globalObj.cdp.onmessage = handler;
    }
  );
  return new CdpServer(cdpBinding);
}

function _createBidiServer() {
  const bidiBinding = new ServerBinding(
    (message) => {
      globalObj.sendBidiResponse(message);
    },
    (handler) => {
      globalObj.onBidiMessage = handler;
    }
  );
  return new BidiServer(bidiBinding);
}

// Needed to filter out info related to BiDi target.
async function _waitSelfTargetId(): Promise<string> {
  return await new Promise((resolve) => {
    globalObj.setSelfTargetId = function (targetId) {
      logSystem('current target ID: ' + targetId);
      resolve(targetId);
    };
  });
}

async function _prepareCdp(cdpServer) {
  // Needed to get events about new targets.
  await cdpServer.sendMessage({
    method: 'Target.setDiscoverTargets',
    params: { discover: true },
  });

  // Needed to automatically attach to new targets.
  await cdpServer.sendMessage({
    method: 'Target.setAutoAttach',
    params: {
      autoAttach: true,
      waitForDebuggerOnStart: false,
      flatten: true,
    },
  });
}
