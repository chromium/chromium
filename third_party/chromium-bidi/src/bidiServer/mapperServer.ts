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

import Protocol from 'devtools-protocol';
import WebSocket from 'ws';
import debug from 'debug';

import {CdpClient, CdpConnection, WebSocketTransport} from '../cdp/index.js';
import {LogType} from '../utils/log.js';

const debugInternal = debug('bidiMapper:internal');
const debugLog = debug('bidiMapper:log');
const mapperDebugLogOthers = debug('bidiMapper:mapperDebug:others');

const bidiMapperMapperDebugPrefix = 'bidiMapper:mapperDebug:';

export class MapperServer {
  #handlers: ((message: string) => void)[] = [];

  #cdpConnection: CdpConnection;
  #mapperCdpClient: CdpClient;

  static async create(
    cdpUrl: string,
    mapperContent: string,
    verbose: boolean
  ): Promise<MapperServer> {
    const cdpConnection = await this.#establishCdpConnection(cdpUrl);
    try {
      const mapperCdpClient = await this.#initMapper(
        cdpConnection,
        mapperContent,
        verbose
      );
      return new MapperServer(cdpConnection, mapperCdpClient);
    } catch (e) {
      cdpConnection.close();
      throw e;
    }
  }

  private constructor(
    cdpConnection: CdpConnection,
    mapperCdpClient: CdpClient
  ) {
    this.#cdpConnection = cdpConnection;
    this.#mapperCdpClient = mapperCdpClient;

    this.#mapperCdpClient.on('Runtime.bindingCalled', this.#onBindingCalled);
    this.#mapperCdpClient.on(
      'Runtime.consoleAPICalled',
      this.#onConsoleAPICalled
    );
    // Catch unhandled exceptions in the mapper.
    this.#mapperCdpClient.on(
      'Runtime.exceptionThrown',
      this.#onRuntimeExceptionThrown
    );
  }

  setOnMessage(handler: (message: string) => void): void {
    this.#handlers.push(handler);
  }

  sendMessage(messageJson: string): Promise<void> {
    return this.#sendBidiMessage(messageJson);
  }

  close() {
    this.#cdpConnection.close();
  }

  static #establishCdpConnection(cdpUrl: string): Promise<CdpConnection> {
    return new Promise((resolve, reject) => {
      debugInternal('Establishing session with cdpUrl: ', cdpUrl);

      const ws = new WebSocket(cdpUrl);

      ws.once('error', reject);

      ws.on('open', () => {
        debugInternal('Session established.');

        const transport = new WebSocketTransport(ws);
        const connection = new CdpConnection(transport);
        resolve(connection);
      });
    });
  }

  async #sendBidiMessage(bidiMessageJson: string): Promise<void> {
    await this.#mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: `onBidiMessage(${JSON.stringify(bidiMessageJson)})`,
    });
  }

  #onBidiMessage(bidiMessage: string): void {
    for (const handler of this.#handlers) handler(bidiMessage);
  }

  #onBindingCalled = (params: Protocol.Runtime.BindingCalledEvent) => {
    if (params.name === 'sendBidiResponse') {
      this.#onBidiMessage(params.payload);
    }
    if (params.name === 'sendDebugMessage') {
      this.#onDebugMessage(params.payload);
    }
  };

  #onDebugMessage = (debugMessageStr: string) => {
    try {
      const debugMessage = JSON.parse(debugMessageStr) as {
        logType: LogType;
        messages: unknown[];
      };

      // BiDi traffic is logged in `bidiServer:SEND ▸`
      if (debugMessage.logType === LogType.bidi) {
        return;
      }

      if (
        debugMessage.logType !== undefined &&
        debugMessage.messages !== undefined
      ) {
        debug(bidiMapperMapperDebugPrefix + debugMessage.logType)(
          // No formatter is needed as the messages will be formatted
          // automatically.
          '',
          ...debugMessage.messages
        );
        return;
      }
    } catch {}
    // Fall back to raw log in case of unknown
    mapperDebugLogOthers(debugMessageStr);
  };

  #onConsoleAPICalled = (params: Protocol.Runtime.ConsoleAPICalledEvent) => {
    debugLog(
      'consoleAPICalled %s %O',
      params.type,
      params.args.map((arg) => arg.value)
    );
  };

  #onRuntimeExceptionThrown = (
    params: Protocol.Runtime.ExceptionThrownEvent
  ) => {
    debugLog('exceptionThrown', params);
  };

  static async #initMapper(
    cdpConnection: CdpConnection,
    mapperContent: string,
    verbose: boolean
  ): Promise<CdpClient> {
    debugInternal('Connection opened.');

    const browserClient = cdpConnection.browserClient();

    const {targetId} = await browserClient.sendCommand('Target.createTarget', {
      url: 'about:blank',
    });
    const {sessionId: mapperSessionId} = await browserClient.sendCommand(
      'Target.attachToTarget',
      {targetId, flatten: true}
    );

    const mapperCdpClient = cdpConnection.getCdpClient(mapperSessionId);

    await mapperCdpClient.sendCommand('Runtime.enable');

    await browserClient.sendCommand('Target.exposeDevToolsProtocol', {
      bindingName: 'cdp',
      targetId,
    });

    await mapperCdpClient.sendCommand('Runtime.addBinding', {
      name: 'sendBidiResponse',
    });

    if (verbose) {
      // Needed to request verbose logs from Mapper.
      await mapperCdpClient.sendCommand('Runtime.addBinding', {
        name: 'sendDebugMessage',
      });
    }

    const launchedPromise = new Promise<void>((resolve, reject) => {
      const onBindingCalled = ({
        name,
        payload,
      }: Protocol.Runtime.BindingCalledEvent) => {
        // Needed to check when Mapper is launched on the frontend.
        if (name === 'sendBidiResponse') {
          try {
            const parsed = JSON.parse(payload);
            if (parsed.launched) {
              mapperCdpClient.off('Runtime.bindingCalled', onBindingCalled);
              resolve();
            }
          } catch (e) {
            reject(new Error('Could not parse initial bidi response as JSON'));
          }
        }
      };

      mapperCdpClient.on('Runtime.bindingCalled', onBindingCalled);
    });

    await mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: mapperContent,
    });

    // Let Mapper know what is it's TargetId to filter out related targets.
    await mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: `window.setSelfTargetId(${JSON.stringify(targetId)})`,
    });

    await launchedPromise;
    debugInternal('Launched!');
    return mapperCdpClient;
  }
}
