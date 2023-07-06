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
import type {Protocol} from 'devtools-protocol';

import {ChromiumBidi, Log, Script} from '../../../protocol/protocol.js';
import type {IEventManager} from '../events/EventManager.js';
import type {Realm} from '../script/realm.js';
import type {RealmStorage} from '../script/realmStorage.js';
import type {CdpTarget} from '../context/cdpTarget.js';

import {getRemoteValuesText} from './logHelper.js';

/** Converts CDP StackTrace object to BiDi StackTrace object. */
function getBidiStackTrace(
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

  return stackFrames ? {callFrames: stackFrames} : undefined;
}

function getLogLevel(consoleApiType: string): Log.Level {
  if ([Log.Level.Error, 'assert'].includes(consoleApiType)) {
    return Log.Level.Error;
  }
  if ([Log.Level.Debug, 'trace'].includes(consoleApiType)) {
    return Log.Level.Debug;
  }
  if ([Log.Level.Warn, 'warning'].includes(consoleApiType)) {
    return Log.Level.Warn;
  }
  return Log.Level.Info;
}

export class LogManager {
  readonly #eventManager: IEventManager;
  readonly #realmStorage: RealmStorage;
  readonly #cdpTarget: CdpTarget;

  private constructor(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    eventManager: IEventManager
  ) {
    this.#cdpTarget = cdpTarget;
    this.#realmStorage = realmStorage;
    this.#eventManager = eventManager;
  }

  static create(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    eventManager: IEventManager
  ) {
    const logManager = new LogManager(cdpTarget, realmStorage, eventManager);

    logManager.#initialize();
    return logManager;
  }

  #initialize() {
    this.#initializeEntryAddedEventListener();
  }

  #initializeEntryAddedEventListener() {
    this.#cdpTarget.cdpClient.on(
      'Runtime.consoleAPICalled',
      (params: Protocol.Runtime.ConsoleAPICalledEvent) => {
        // Try to find realm by `cdpSessionId` and `executionContextId`,
        // if provided.
        const realm: Realm | undefined = this.#realmStorage.findRealm({
          cdpSessionId: this.#cdpTarget.cdpSessionId,
          executionContextId: params.executionContextId,
        });
        const argsPromise: Promise<Script.RemoteValue[]> =
          realm === undefined
            ? Promise.resolve(params.args as Script.RemoteValue[])
            : // Properly serialize arguments if possible.
              Promise.all(
                params.args.map((arg) => {
                  return realm.serializeCdpObject(
                    arg,
                    Script.ResultOwnership.None
                  );
                })
              );

        this.#eventManager.registerPromiseEvent(
          argsPromise.then((args) => ({
            method: ChromiumBidi.Log.EventNames.LogEntryAddedEvent,
            params: {
              level: getLogLevel(params.type),
              source: {
                realm: realm?.realmId ?? 'UNKNOWN',
                context: realm?.browsingContextId ?? 'UNKNOWN',
              },
              text: getRemoteValuesText(args, true),
              timestamp: Math.round(params.timestamp),
              stackTrace: getBidiStackTrace(params.stackTrace),
              type: 'console',
              // Console method is `warn`, not `warning`.
              method: params.type === 'warning' ? 'warn' : params.type,
              args,
            },
          })),
          realm?.browsingContextId ?? 'UNKNOWN',
          ChromiumBidi.Log.EventNames.LogEntryAddedEvent
        );
      }
    );

    this.#cdpTarget.cdpClient.on(
      'Runtime.exceptionThrown',
      (params: Protocol.Runtime.ExceptionThrownEvent) => {
        // Try to find realm by `cdpSessionId` and `executionContextId`,
        // if provided.
        const realm: Realm | undefined = this.#realmStorage.findRealm({
          cdpSessionId: this.#cdpTarget.cdpSessionId,
          executionContextId: params.exceptionDetails.executionContextId,
        });

        // Try all the best to get the exception text.
        const textPromise = (async () => {
          if (!params.exceptionDetails.exception) {
            return params.exceptionDetails.text;
          }
          if (realm === undefined) {
            return JSON.stringify(params.exceptionDetails.exception);
          }
          return realm.stringifyObject(params.exceptionDetails.exception);
        })();

        this.#eventManager.registerPromiseEvent(
          textPromise.then((text) => ({
            method: ChromiumBidi.Log.EventNames.LogEntryAddedEvent,
            params: {
              level: Log.Level.Error,
              source: {
                realm: realm?.realmId ?? 'UNKNOWN',
                context: realm?.browsingContextId ?? 'UNKNOWN',
              },
              text,
              timestamp: Math.round(params.timestamp),
              stackTrace: getBidiStackTrace(params.exceptionDetails.stackTrace),
              type: 'javascript',
            },
          })),
          realm?.browsingContextId ?? 'UNKNOWN',
          ChromiumBidi.Log.EventNames.LogEntryAddedEvent
        );
      }
    );
  }
}
