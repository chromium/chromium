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

import { CdpClient } from '../../../cdp';
import { IBidiServer } from '../../utils/bidiServer';
import { Log, Script } from '../protocol/bidiProtocolTypes';
import { getRemoteValuesText } from './logHelper';
import { ScriptEvaluator } from '../script/scriptEvaluator';
import { Protocol } from 'devtools-protocol';
import { Realm } from '../script/realm';

export class LogManager {
  readonly #contextId: string;
  readonly #cdpClient: CdpClient;
  readonly #bidiServer: IBidiServer;
  readonly #cdpSessionId: string;

  private constructor(
    contextId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    bidiServer: IBidiServer
  ) {
    this.#cdpSessionId = cdpSessionId;
    this.#bidiServer = bidiServer;
    this.#cdpClient = cdpClient;
    this.#contextId = contextId;
  }

  public static create(
    contextId: string,
    cdpClient: CdpClient,
    cdpSessionId: string,
    bidiServer: IBidiServer
  ) {
    const logManager = new LogManager(
      contextId,
      cdpClient,
      cdpSessionId,
      bidiServer
    );

    logManager.#initialize();
    return logManager;
  }

  #initialize() {
    this.#initializeEventListeners();
  }

  #initializeEventListeners() {
    this.#initializeLogEntryAddedEventListener();
  }

  #initializeLogEntryAddedEventListener() {
    this.#cdpClient.Runtime.on(
      'consoleAPICalled',
      async (params: Protocol.Runtime.ConsoleAPICalledEvent) => {
        const realm = Realm.getRealm({
          cdpSessionId: this.#cdpSessionId,
          executionContextId: params.executionContextId,
        });
        const args = await Promise.all(
          params.args.map(async (arg) => {
            return realm.serializeCdpObject(arg, 'none');
          })
        );

        await this.#bidiServer.sendMessage(
          new Log.LogEntryAddedEvent({
            level: LogManager.#getLogLevel(params.type),
            source: {
              realm: params.executionContextId.toString(),
              context: this.#contextId,
            },
            text: getRemoteValuesText(args, true),
            timestamp: Math.round(params.timestamp),
            stackTrace: LogManager.#getBidiStackTrace(params.stackTrace),
            type: 'console',
            // Console method is `warn`, not `warning`.
            method: params.type === 'warning' ? 'warn' : params.type,
            args: args,
          })
        );
      }
    );

    this.#cdpClient.Runtime.on(
      'exceptionThrown',
      async (params: Protocol.Runtime.ExceptionThrownEvent) => {
        // Try to find realm by `cdpSessionId` and `executionContextId`,
        // if provided.
        const realm: Realm | undefined = (() => {
          if (params.exceptionDetails.executionContextId !== undefined) {
            return Realm.getRealm({
              cdpSessionId: this.#cdpSessionId,
              executionContextId: params.exceptionDetails.executionContextId,
            });
          }
          return undefined;
        })();

        // Try all the best to get the exception text.
        const text = await (async () => {
          if (!params.exceptionDetails.exception) {
            return params.exceptionDetails.text;
          }
          if (realm === undefined) {
            return JSON.stringify(params.exceptionDetails.exception);
          }
          return await realm.stringifyObject(
            params.exceptionDetails.exception,
            realm
          );
        })();

        await this.#bidiServer.sendMessage(
          new Log.LogEntryAddedEvent({
            level: 'error',
            source: {
              realm: realm?.realmId ?? 'UNKNOWN',
              context: realm?.browsingContextId ?? 'UNKNOWN',
            },
            text,
            timestamp: Math.round(params.timestamp),
            stackTrace: LogManager.#getBidiStackTrace(
              params.exceptionDetails.stackTrace
            ),
            type: 'javascript',
          })
        );
      }
    );
  }

  static #getLogLevel(consoleApiType: string): Log.LogLevel {
    if (['assert', 'error'].includes(consoleApiType)) {
      return 'error';
    }
    if (['debug', 'trace'].includes(consoleApiType)) {
      return 'debug';
    }
    if (['warn', 'warning'].includes(consoleApiType)) {
      return 'warning';
    }
    return 'info';
  }

  // convert CDP StackTrace object to Bidi StackTrace object
  static #getBidiStackTrace(
    cdpStackTrace: Protocol.Runtime.StackTrace | undefined
  ): Script.StackTrace | undefined {
    const stackFrames = cdpStackTrace?.callFrames.map((callFrame) => {
      return {
        columnNumber: callFrame.columnNumber,
        functionName: callFrame.functionName,
        lineNumber: callFrame.lineNumber,
        url: callFrame.url,
      };
    });

    return stackFrames ? { callFrames: stackFrames } : undefined;
  }
}
