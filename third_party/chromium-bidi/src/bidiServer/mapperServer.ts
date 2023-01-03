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

import debug from 'debug';

const debugInternal = debug('bidiMapper:internal');
const debugLog = debug('bidiMapper:log');

import WebSocket from 'ws';
import Protocol from 'devtools-protocol';

import {CdpClient, CdpConnection, WebSocketTransport} from '../cdp/index.js';

export class MapperServer {
  private _handlers: ((message: string) => void)[] = new Array();

  static async create(
    cdpUrl: string,
    mapperContent: string
  ): Promise<MapperServer> {
    const cdpConnection = await this._establishCdpConnection(cdpUrl);
    try {
      const mapperCdpClient = await this._initMapper(
        cdpConnection,
        mapperContent
      );
      return new MapperServer(cdpConnection, mapperCdpClient);
    } catch (e) {
      cdpConnection.close();
      throw e;
    }
  }

  private constructor(
    private _cdpConnection: CdpConnection,
    private _mapperCdpClient: CdpClient
  ) {
    this._mapperCdpClient.on('Runtime.bindingCalled', this._onBindingCalled);
    this._mapperCdpClient.on(
      'Runtime.consoleAPICalled',
      this._onConsoleAPICalled
    );
  }

  setOnMessage(handler: (message: string) => void): void {
    this._handlers.push(handler);
  }
  sendMessage(messageJson: string): Promise<void> {
    return this._sendBidiMessage(messageJson);
  }
  close() {
    this._cdpConnection.close();
  }

  private static async _establishCdpConnection(
    cdpUrl: string
  ): Promise<CdpConnection> {
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

  private async _sendBidiMessage(bidiMessageJson: string): Promise<void> {
    await this._mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: 'onBidiMessage(' + JSON.stringify(bidiMessageJson) + ')',
    });
  }

  private _onBidiMessage(bidiMessage: string): void {
    for (const handler of this._handlers) handler(bidiMessage);
  }

  private _onBindingCalled = async (
    params: Protocol.Runtime.BindingCalledEvent
  ) => {
    if (params.name === 'sendBidiResponse') {
      this._onBidiMessage(params.payload);
    }
  };

  private _onConsoleAPICalled = async (
    params: Protocol.Runtime.ConsoleAPICalledEvent
  ) => {
    debugLog(
      'consoleAPICalled %s %O',
      params.type,
      params.args.map((arg) => arg.value)
    );
  };

  private static async _initMapper(
    cdpConnection: CdpConnection,
    mapperContent: string
  ): Promise<CdpClient> {
    debugInternal('Connection opened.');

    // await browserClient.Log.enable();

    const browserClient = cdpConnection.browserClient();

    const {targetId} = await browserClient.sendCommand('Target.createTarget', {
      url: 'about:blank',
    });
    const {sessionId: mapperSessionId} = await browserClient.sendCommand(
      'Target.attachToTarget',
      {targetId, flatten: true}
    );

    const mapperCdpClient = cdpConnection.getCdpClient(mapperSessionId);
    if (!mapperCdpClient) {
      throw new Error('Unable to connect to mapper CDP target');
    }

    await mapperCdpClient.sendCommand('Runtime.enable');

    await browserClient.sendCommand('Target.exposeDevToolsProtocol', {
      bindingName: 'cdp',
      targetId,
    });

    await mapperCdpClient.sendCommand('Runtime.addBinding', {
      name: 'sendBidiResponse',
    });

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
      expression: 'window.setSelfTargetId(' + JSON.stringify(targetId) + ')',
    });

    await launchedPromise;
    debugInternal('Launched!');
    return mapperCdpClient;
  }
}
