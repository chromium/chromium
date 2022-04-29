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
import { Log, Script } from '../../bidiProtocolTypes';
import { getRemoteValuesText } from './logHelper';
import { Serializer } from './serializer';
import { Protocol } from 'devtools-protocol';

export class LogManager {
  private constructor(
    private _contextId: string,
    private _cdpClient: CdpClient,
    private _bidiServer: IBidiServer,
    private _serializer: Serializer
  ) {}

  public static async create(
    contextId: string,
    cdpClient: CdpClient,
    bidiServer: IBidiServer,
    serializer: Serializer
  ) {
    const logManager = new LogManager(
      contextId,
      cdpClient,
      bidiServer,
      serializer
    );

    logManager._initialize();
    return logManager;
  }

  private async _initialize() {
    this._initializeEventListeners();
  }

  private _initializeEventListeners() {
    this._initializeLogEntryAddedEventListener();
  }

  private _initializeLogEntryAddedEventListener() {
    this._cdpClient.Runtime.on('consoleAPICalled', async (params) => {
      const args = await Promise.all(
        params.args.map(async (arg) => {
          return await this._serializer?.serializeCdpObject(arg);
        })
      );

      await this._bidiServer.sendMessage({
        method: 'log.entryAdded',
        params: {
          level: this._getLogLevel(params.type),
          text: getRemoteValuesText(args, true),
          timestamp: params.timestamp,
          stackTrace: this._getBidiStackTrace(params.stackTrace),
          type: 'console',
          method: params.type,
          realm: this._contextId,
          args: args,
        },
      });
    });

    this._cdpClient.Runtime.on('exceptionThrown', async (params) => {
      let text = params.exceptionDetails.text;

      if (params.exceptionDetails.exception) {
        const exceptionString = await this._serializer.stringifyCdpException(
          params.exceptionDetails.exception
        );
        if (exceptionString) {
          text = exceptionString;
        }
      }

      await this._bidiServer.sendMessage({
        method: 'log.entryAdded',
        params: {
          level: 'error',
          text,
          timestamp: params.timestamp,
          stackTrace: this._getBidiStackTrace(
            params.exceptionDetails.stackTrace
          ),
          type: 'javascript',
          realm: this._contextId,
        },
      });
    });
  }

  private _getLogLevel(consoleAPIType: string): Log.LogLevel {
    if (consoleAPIType in ['error', 'assert']) {
      return 'error';
    }
    if (consoleAPIType in ['debug', 'trace']) {
      return 'debug';
    }
    if (consoleAPIType in ['warning', 'warn']) {
      return 'warning';
    }
    return 'info';
  }

  // convert CDP StackTrace object to Bidi StackTrace object
  private _getBidiStackTrace(
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

    const bidiStackTrace = stackFrames
      ? {
          callFrames: stackFrames,
        }
      : undefined;

    return bidiStackTrace;
  }
}
