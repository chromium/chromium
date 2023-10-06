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
import type {LogType, LogPrefix} from '../utils/log.js';
import {EventEmitter} from '../utils/EventEmitter.js';

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

export class MapperCdpConnection extends EventEmitter<
  Record<'message', string>
> {
  #cdpConnection: CdpConnection;
  #mapperCdpClient: CdpClient;

  static async create(
    cdpConnection: CdpConnection,
    mapperContent: string,
    verbose: boolean
  ): Promise<MapperCdpConnection> {
    try {
      const mapperCdpClient = await this.#initMapper(
        cdpConnection,
        mapperContent,
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
    super();
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

  async sendMessage(message: string): Promise<void> {
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

  #onBidiMessage(message: string): void {
    this.emit('message', message);
  }

  #onBindingCalled = (params: Protocol.Runtime.BindingCalledEvent) => {
    if (params.name === 'sendBidiResponse') {
      this.#onBidiMessage(params.payload);
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

    // Let Mapper know its TargetId to filter out related targets.
    await mapperCdpClient.sendCommand('Runtime.evaluate', {
      expression: `window.setSelfTargetId('${targetId}')`,
    });

    await launchedPromise;
    debugInternal('Launched!');
    return mapperCdpClient;
  }
}
