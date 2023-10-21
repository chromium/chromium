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

import {BidiServer, OutgoingMessage} from '../bidiMapper/BidiMapper.js';
import {CdpConnection} from '../cdp/CdpConnection.js';
import {LogType} from '../utils/log.js';

import {BidiParser} from './BidiParser.js';
import {WindowBidiTransport, WindowCdpTransport} from './Transport.js';
import {generatePage, log} from './mapperTabPage.js';

declare global {
  interface Window {
    // `runMapper` function will be defined by the Mapper in the Tab, and will
    // be evaluated via `Runtime.evaluate` by the Node runner, providing all the
    // required parameters.
    runMapperInstance: ((...args: any) => Promise<void>) | null;

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

    /**
     * @deprecated Use `runMapperInstance` instead. Used for backward compatibility
     * with ChromeDriver.
     */
    // TODO: Remove this after https://crrev.com/c/4952609 reaches stable.
    setSelfTargetId: (targetId: string) => void;
  }
}

generatePage();
const mapperTabToServerTransport = new WindowBidiTransport();
const cdpTransport = new WindowCdpTransport();
/**
 * A CdpTransport implementation that uses the window.cdp bindings
 * injected by Target.exposeDevToolsProtocol.
 */
const cdpConnection = new CdpConnection(cdpTransport, log);

/**
 * Launches the BiDi mapper instance.
 * @param {string} selfTargetId
 */
async function runMapperInstance(selfTargetId: string) {
  console.log('Launching Mapper instance with selfTargetId:', selfTargetId);

  const bidiServer = await BidiServer.createAndStart(
    mapperTabToServerTransport,
    cdpConnection,
    /**
     * Create a Browser CDP Session per Mapper instance.
     */
    await cdpConnection.createBrowserSession(),
    selfTargetId,
    new BidiParser(),
    log
  );

  log(LogType.debugInfo, 'Mapper instance has been launched');

  return bidiServer;
}

/**
 * Set `window.runMapper` to a function which launches the BiDi mapper instance.
 * @param selfTargetId Needed to filter out info related to BiDi target.
 */
window.runMapperInstance = async (selfTargetId) => {
  await runMapperInstance(selfTargetId);
};

/**
 * @deprecated Use `runMapperInstance` instead. Used for backward compatibility
 * with ChromeDriver.
 */
// TODO: Remove this after https://crrev.com/c/4952609 reaches stable.
window.setSelfTargetId = async (selfTargetId) => {
  const bidiServer = await runMapperInstance(selfTargetId);
  bidiServer.emitOutgoingMessage(
    OutgoingMessage.createResolved({
      launched: true,
    }),
    'launched'
  );
};
