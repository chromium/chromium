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

import {
  type AclHandler,
  BidiServer,
  type BidiTransport,
} from '../bidiMapper/BidiMapper.js';
import {CloseError, type CdpClient} from '../cdp/CdpClient.js';
import {MapperCdpConnection} from '../cdp/CdpConnection.js';
import {LogType} from '../utils/log.js';
import type {Transport} from '../utils/transport.js';

import {BidiParser} from './BidiParser.js';
import {log} from './logger.js';
import {WindowBidiTransport, WindowCdpTransport} from './Transport.js';

declare global {
  interface Window {
    // `runMapperInstance` function will be defined by the Mapper in the Tab,
    // and will be evaluated via `Runtime.evaluate` by the Node runner or
    // ChromeDriver, providing all the required parameters.
    runMapperInstance:
      | ((
          selfTargetId: string,
          bidiTransport?: BidiTransport,
          cdpTransport?: Transport,
          restrictedMode?: boolean,
          aclHandler?: AclHandler,
        ) => Promise<BidiServer>)
      | null;

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
  }
}

const windowBidiTransport = new WindowBidiTransport();
const windowCdpTransport = new WindowCdpTransport();
let windowCdpConnection: MapperCdpConnection | null = null;
let isWindowBidiTransportUsed = false;

function getWindowCdpConnection(): MapperCdpConnection {
  windowCdpConnection ??= new MapperCdpConnection(windowCdpTransport, log);
  return windowCdpConnection;
}

/**
 * Launches the BiDi mapper instance.
 * @param selfTargetId Needed to filter out info related to BiDi target.
 * @param bidiTransport Optional custom BiDi transport for additional Mapper
 * instances; the default WindowBidiTransport can only be used once.
 * @param cdpTransport Optional custom CDP transport for additional Mapper instances.
 * @param restrictedMode Optional flag requiring restricted sessions.
 * @param aclHandler Optional embedder ACL handler for restricted sessions.
 */
async function runMapperInstance(
  selfTargetId: string,
  bidiTransport: BidiTransport = windowBidiTransport,
  cdpTransport: Transport = windowCdpTransport,
  restrictedMode = false,
  aclHandler?: AclHandler,
): Promise<BidiServer> {
  const usesWindowBidi = bidiTransport === windowBidiTransport;
  if (usesWindowBidi) {
    if (isWindowBidiTransportUsed) {
      throw new Error(
        'Default WindowBidiTransport is already in use; pass a custom bidiTransport.',
      );
    }
    isWindowBidiTransportUsed = true;
  }

  // eslint-disable-next-line no-console
  console.log('Launching Mapper instance with selfTargetId:', selfTargetId);

  const usesWindowCdp = cdpTransport === windowCdpTransport;
  /**
   * Reuse a single MapperCdpConnection over `window.cdp` when using the default
   * transport so additional mappers do not overwrite `windowCdpTransport`'s
   * message handler; create a dedicated connection only for custom transports.
   */
  const cdpConnection = usesWindowCdp
    ? getWindowCdpConnection()
    : new MapperCdpConnection(cdpTransport, log);

  let browserCdpClient: CdpClient | undefined;
  const cleanupCdp = () => {
    if (browserCdpClient?.sessionId !== undefined) {
      void cdpConnection
        .closeBrowserSession(browserCdpClient.sessionId)
        .catch((error) => {
          if (!(error instanceof CloseError)) {
            log(LogType.debugError)?.(error);
          }
        });
    }
    if (!usesWindowCdp) {
      cdpConnection.close();
    }
  };

  let bidiServer: BidiServer;
  try {
    /**
     * Create a Browser CDP Session per Mapper instance.
     */
    browserCdpClient = await cdpConnection.createBrowserSession();
    bidiServer = await BidiServer.createAndStart(
      bidiTransport,
      cdpConnection,
      browserCdpClient,
      selfTargetId,
      new BidiParser(),
      log,
      restrictedMode === true,
      aclHandler,
    );
  } catch (error) {
    if (usesWindowBidi) {
      isWindowBidiTransportUsed = false;
    }
    cleanupCdp();
    throw error;
  }

  bidiServer.once('close', cleanupCdp);

  log(LogType.debugInfo)?.('Mapper instance has been launched');

  return bidiServer;
}

/**
 * Set `window.runMapperInstance` to a function which launches the BiDi mapper
 * instance.
 * @param selfTargetId Needed to filter out info related to BiDi target.
 * @param bidiTransport Optional custom BiDi transport for additional Mapper
 * instances; the default WindowBidiTransport can only be used once.
 * @param cdpTransport Optional custom CDP transport for additional Mapper instances.
 * @param restrictedMode Optional flag requiring restricted sessions.
 * @param aclHandler Optional embedder ACL handler for restricted sessions.
 */
window.runMapperInstance = async (
  selfTargetId,
  bidiTransport,
  cdpTransport,
  restrictedMode,
  aclHandler,
) => {
  return await runMapperInstance(
    selfTargetId,
    bidiTransport,
    cdpTransport,
    restrictedMode,
    aclHandler,
  );
};
