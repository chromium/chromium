/**
 * Copyright 2021 Google LLC.
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

import { CommandProcessor } from './commandProcessor';

import { CdpClient, CdpConnection } from '../cdp';
import { BidiServer } from './utils/bidiServer';
import { ITransport } from '../utils/transport';

import { log } from '../utils/log';
const logSystem = log('system');

declare global {
  interface Window {
    //`window.cdp` is exposed by `Target.exposeDevToolsProtocol` from the server side.
    // https://chromedevtools.github.io/devtools-protocol/tot/Target/#method-exposeDevToolsProtocol
    cdp: {
      send: (message: string) => void;
      onmessage: ((message: string) => void) | null;
    };

    // `window.sendBidiResponse` is exposed by `Runtime.addBinding` from the server side.
    sendBidiResponse: (response: string) => void;

    // `window.onBidiMessage` is called via `Runtime.evaluate` from the server side.
    onBidiMessage: ((message: string) => void) | null;

    // `window.setSelfTargetId` is called via `Runtime.evaluate` from the server side.
    setSelfTargetId: (targetId: string) => void;
  }
}

// Initiate `setSelfTargetId` as soon as possible to prevent race condition.
const _waitSelfTargetIdPromise = _waitSelfTargetId();

(async () => {
  window.document.documentElement.innerHTML = `<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2>`;
  window.document.title = 'BiDi Mapper';

  const cdpConnection = _createCdpConnection();
  const cdpClient = cdpConnection.browserClient();
  const bidiServer = _createBidiServer();

  // Needed to filter out info related to BiDi target.
  const selfTargetId = await _waitSelfTargetIdPromise;

  // Needed to get events about new targets.
  await _prepareCdp(cdpClient);

  CommandProcessor.run(cdpClient, bidiServer, selfTargetId);

  logSystem('launched');

  bidiServer.sendMessage({ launched: true });
})();

function _createCdpConnection() {
  // A CdpTransport implementation that uses the window.cdp bindings
  // injected by Target.exposeDevToolsProtocol.
  class WindowCdpTransport implements ITransport {
    private _onMessage: ((message: string) => void) | null = null;

    constructor() {
      window.cdp.onmessage = (message: string) => {
        if (this._onMessage) {
          this._onMessage.call(null, message);
        }
      };
    }

    setOnMessage(onMessage: (message: string) => Promise<void>): void {
      this._onMessage = onMessage;
    }

    async sendMessage(message: string): Promise<void> {
      window.cdp.send(message);
    }

    close() {
      this._onMessage = null;
      window.cdp.onmessage = null;
    }
  }

  return new CdpConnection(new WindowCdpTransport());
}

function _createBidiServer() {
  class WindowBidiTransport implements ITransport {
    private _onMessage: ((message: string) => void) | null = null;

    constructor() {
      window.onBidiMessage = (message: string) => {
        if (this._onMessage) {
          this._onMessage.call(null, message);
        }
      };
    }

    setOnMessage(onMessage: (message: string) => Promise<void>): void {
      this._onMessage = onMessage;
    }

    async sendMessage(message: string): Promise<void> {
      window.sendBidiResponse(message);
    }

    close() {
      this._onMessage = null;
      window.onBidiMessage = null;
    }
  }

  return new BidiServer(new WindowBidiTransport());
}

// Needed to filter out info related to BiDi target.
async function _waitSelfTargetId(): Promise<string> {
  return await new Promise((resolve) => {
    window.setSelfTargetId = function (targetId) {
      logSystem('current target ID: ' + targetId);
      resolve(targetId);
    };
  });
}

async function _prepareCdp(cdpClient: CdpClient) {
  // Needed to get events about new targets.
  await cdpClient.Target.setDiscoverTargets({ discover: true });

  // Needed to automatically attach to new targets.
  await cdpClient.Target.setAutoAttach({
    autoAttach: true,
    waitForDebuggerOnStart: false,
    flatten: true,
  });
}
