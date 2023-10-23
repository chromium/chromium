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

import type Protocol from 'devtools-protocol';
import debug, {type Debugger} from 'debug';

import type {CdpConnection} from '../cdp/CdpConnection.js';
import type {CdpClient} from '../cdp/CdpClient.js';
import type {LogPrefix, LogType} from '../utils/log.js';

import {SimpleTransport} from './SimpleTransport.js';

const debugInternal = debug('bidi:mapper:internal');
const debugInfo = debug('bidi:mapper:info');
const debugOthers = debug('bidi:mapper:debug:others');
// Memorizes a debug creation
const loggers = new Map<string, Debugger>();
const getLogger = (type: LogPrefix) => {
  const prefix = `bidi:mapper:${type}`;
  let logger = loggers.get(prefix);
  if (!logger) {
    logger = debug(prefix);
    loggers.set(prefix, logger);
  }
  return logger;
};

export class MapperCdpConnection {
  #cdpConnection: CdpConnection;
  #mapperCdpClient: CdpClient;
  #bidiSession: SimpleTransport;

  static async create(
    cdpConnection: CdpConnection,
    mapperTabSource: string,
    verbose: boolean
  ): Promise<MapperCdpConnection> {
    try {
      const mapperCdpClient = await this.#initMapper(
        cdpConnection,
        mapperTabSource,
        verbose
      );
      return new MapperCdpConnection(cdpConnection, mapperCdpClient);
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
    this.#bidiSession = new SimpleTransport(
      async (message) => await this.#sendMessage(message)
    );

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

  async #sendMessage(message: string): Promise<void> {
    try {
      await this.#mapperCdpClient.sendCommand('Runtime.evaluate', {
        expression: `onBidiMessage(${JSON.stringify(message)})`,
      });
    } catch (error) {
      debugInternal('Call to onBidiMessage failed', error);
    }
  }

  close() {
    this.#cdpConnection.close();
  }

  bidiSession(): SimpleTransport {
    return this.#bidiSession;
  }

  #onBindingCalled = (params: Protocol.Runtime.BindingCalledEvent) => {
    if (params.name === 'sendBidiResponse') {
      this.#bidiSession.emit('message', params.payload);
    } else if (params.name === 'sendDebugMessage') {
      this.#onDebugMessage(params.payload);
    }
  };

  #onDebugMessage = (json: string) => {
    try {
      const log: {
        logType?: LogType;
        messages?: unknown[];
      } = JSON.parse(json);

      if (log.logType !== undefined && log.messages !== undefined) {
        const logger = getLogger(log.logType);
        logger(log.messages);
      }
    } catch {
      // Fall back to raw log in case of unknown
      debugOthers(json);
    }
  };

  #onConsoleAPICalled = (params: Protocol.Runtime.ConsoleAPICalledEvent) => {
    debugInfo(
      'consoleAPICalled: %s %O',
      params.type,
      params.args.map((arg) => arg.value)
    );
  };

  #onRuntimeExceptionThrown = (
    params: Protocol.Runtime.ExceptionThrownEvent
  ) => {
    debugInfo('exceptionThrown:', params);
  };

  static async #initMapper(
    cdpConnection: CdpConnection,
    mapperTabSource: string,
    verbose: boolean
  ): Promise<CdpClient> {
    debugInternal('Connection opened.');

    const browserClient = await cdpConnection.createBrowserSession();

    const {targetId: mapperTabTargetId} = await browserClient.sendCommand(
      'Target.createTarget',
      {
        url: 'about:blank',
      }
    );
    const {sessionId: mapperSessionId} = await browserClient.sendCommand(
      'Target.attachToTarget',
      {targetId: mapperTabTargetId, flatten: true}
    );

    const mapperCdpClient = cdpConnection.getCdpClient(mapperSessionId);

    await mapperCdpClient.sendCommand('Runtime.enable');

    await browserClient.sendCommand('Target.exposeDevToolsProtocol', {
      bindingName: 'cdp',
      targetId: mapperTabTargetId,
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

    // Evaluate Mapper Tab sources in the tab.
    await mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: mapperTabSource,
    });

    // Run Mapper instance.
    await mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: `window.runMapperInstance('${mapperTabTargetId}')`,
      awaitPromise: true,
    });

    debugInternal('Launched!');
    return mapperCdpClient;
  }
}
