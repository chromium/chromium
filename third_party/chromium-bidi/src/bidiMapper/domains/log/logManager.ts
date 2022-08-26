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

export class LogManager {
  readonly #contextId: string;
  readonly #cdpClient: CdpClient;
  readonly #bidiServer: IBidiServer;
  readonly #serializer: ScriptEvaluator;

  private constructor(
    contextId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    serializer: ScriptEvaluator
  ) {
    this.#serializer = serializer;
    this.#bidiServer = bidiServer;
    this.#cdpClient = cdpClient;
    this.#contextId = contextId;
  }

  public static create(
    contextId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    serializer: ScriptEvaluator
  ) {
    const logManager = new LogManager(
      contextId,
      cdpClient,
      bidiServer,
      serializer
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
        const args = await Promise.all(
          params.args.map(async (arg) => {
            return this.#serializer?.serializeCdpObject(
              arg,
              'none',
              params.executionContextId
            );
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
        let text = params.exceptionDetails.text;

        if (params.exceptionDetails.exception) {
          if (params.exceptionDetails.executionContextId === undefined) {
            text = JSON.stringify(params.exceptionDetails.exception);
          } else {
            const exceptionString = await this.#serializer.stringifyObject(
              params.exceptionDetails.exception,
              params.exceptionDetails.executionContextId!
            );
            if (exceptionString) {
              text = exceptionString;
            }
          }
        }

        await this.#bidiServer.sendMessage(
          new Log.LogEntryAddedEvent({
            level: 'error',
            source: {
              // TODO sadym: add proper realm handling.
              realm: (
                params.exceptionDetails.executionContextId ?? 'UNKNOWN'
              ).toString(),
              context: this.#contextId,
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
