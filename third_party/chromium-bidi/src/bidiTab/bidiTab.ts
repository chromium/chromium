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
 *
 * @license
 */

import {BidiServer, OutgoingMessage} from '../bidiMapper/bidiMapper.js';
import {CdpConnection} from '../cdp/CdpConnection.js';
import {LogType} from '../utils/log.js';

import {BidiParser} from './BidiParser.js';
import {WindowBidiTransport, WindowCdpTransport} from './Transport.js';
import {generatePage, log} from './mapperTabPage.js';

declare global {
  interface Window {
    // `window.cdp` is exposed by `Target.exposeDevToolsProtocol` from the server side.
    // https://chromedevtools.github.io/devtools-protocol/tot/Target/#method-exposeDevToolsProtocol
    cdp: {
      send: (message: string) => void;
      onmessage: ((message: string) => void) | null;
    };

    // `window.sendBidiResponse` is exposed by `Runtime.addBinding` from the server side.
    sendBidiResponse: (response: string) => void;

    // `window.onBidiMessage` is called via `Runtime.evaluate` from the server side.
    onBidiMessage: ((message: string) => void) | null;

    // Set from the server side if verbose logging is required.
    sendDebugMessage?: ((message: string) => void) | null;

    // `window.setSelfTargetId` is called via `Runtime.evaluate` from the server side.
    setSelfTargetId: (targetId: string) => void;
  }
}

// Initiate `setSelfTargetId` as soon as possible to prevent race condition.
const waitSelfTargetIdPromise = waitSelfTargetId();

void (async () => {
  generatePage();

  // Needed to filter out info related to BiDi target.
  const selfTargetId = await waitSelfTargetIdPromise;

  const bidiServer = await BidiServer.createAndStart(
    new WindowBidiTransport(),
    /**
     * A CdpTransport implementation that uses the window.cdp bindings
     * injected by Target.exposeDevToolsProtocol.
     */
    new CdpConnection(new WindowCdpTransport(), log),
    selfTargetId,
    new BidiParser(),
    log
  );

  log(LogType.debugInfo, 'Launched');

  bidiServer.emitOutgoingMessage(
    OutgoingMessage.createResolved({
      launched: true,
    }),
    'launched'
  );
})();

// Needed to filter out info related to BiDi target.
async function waitSelfTargetId(): Promise<string> {
  return new Promise((resolve) => {
    window.setSelfTargetId = (targetId) => {
      log(LogType.debugInfo, 'Current target ID:', targetId);
      resolve(targetId);
    };
  });
}
