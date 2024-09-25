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
import {LogType, type LoggerFn} from '../../../utils/log.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {Realm} from '../script/Realm.js';
import type {RealmStorage} from '../script/RealmStorage.js';
import type {EventManager} from '../session/EventManager.js';

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

function getLogMethod(consoleApiType: string): string {
  switch (consoleApiType) {
    case 'warning':
      return 'warn';
    case 'startGroup':
      return 'group';
    case 'startGroupCollapsed':
      return 'groupCollapsed';
    case 'endGroup':
      return 'groupEnd';
  }

  return consoleApiType;
}

export class LogManager {
  readonly #eventManager: EventManager;
  readonly #realmStorage: RealmStorage;
  readonly #cdpTarget: CdpTarget;
  readonly #logger?: LoggerFn;

  private constructor(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    eventManager: EventManager,
    logger?: LoggerFn
  ) {
    this.#cdpTarget = cdpTarget;
    this.#realmStorage = realmStorage;
    this.#eventManager = eventManager;
    this.#logger = logger;
  }

  static create(
    cdpTarget: CdpTarget,
    realmStorage: RealmStorage,
    eventManager: EventManager,
    logger?: LoggerFn
  ) {
    const logManager = new LogManager(
      cdpTarget,
      realmStorage,
      eventManager,
      logger
    );

    logManager.#initializeEntryAddedEventListener();

    return logManager;
  }

  /**
   * Heuristic serialization of CDP remote object. If possible, return the BiDi value
   * without deep serialization.
   */
  async #heuristicSerializeArg(
    arg: Protocol.Runtime.RemoteObject,
    realm: Realm
  ): Promise<Script.RemoteValue> {
    switch (arg.type) {
      // TODO: Implement regexp, array, object, map and set heuristics base on
      //  preview.
      case 'undefined':
        return {type: 'undefined'};
      case 'boolean':
        return {type: 'boolean', value: arg.value};
      case 'string':
        return {type: 'string', value: arg.value};
      case 'number':
        // The value can be either a number or a string like `Infinity` or `-0`.
        return {type: 'number', value: arg.unserializableValue ?? arg.value};
      case 'bigint':
        if (
          arg.unserializableValue !== undefined &&
          arg.unserializableValue[arg.unserializableValue.length - 1] === 'n'
        ) {
          return {
            type: arg.type,
            value: arg.unserializableValue.slice(0, -1),
          };
        }
        // Unexpected bigint value, fall back to CDP deep serialization.
        break;
      case 'object':
        if (arg.subtype === 'null') {
          return {type: 'null'};
        }
        // Fall back to CDP deep serialization.
        break;
      default:
        // Fall back to CDP deep serialization.
        break;
    }
    // Fall back to CDP deep serialization.
    return await realm.serializeCdpObject(arg, Script.ResultOwnership.None);
  }

  #initializeEntryAddedEventListener() {
    this.#cdpTarget.cdpClient.on('Runtime.consoleAPICalled', (params) => {
      // Try to find realm by `cdpSessionId` and `executionContextId`,
      // if provided.
      const realm: Realm | undefined = this.#realmStorage.findRealm({
        cdpSessionId: this.#cdpTarget.cdpSessionId,
        executionContextId: params.executionContextId,
      });
      if (realm === undefined) {
        // Ignore exceptions not attached to any realm.
        this.#logger?.(LogType.cdp, params);
        return;
      }

      const argsPromise: Promise<Script.RemoteValue[]> = Promise.all(
        params.args.map((arg) => this.#heuristicSerializeArg(arg, realm))
      );

      for (const browsingContext of realm.associatedBrowsingContexts) {
        this.#eventManager.registerPromiseEvent(
          argsPromise.then(
            (args) => ({
              kind: 'success',
              value: {
                type: 'event',
                method: ChromiumBidi.Log.EventNames.LogEntryAdded,
                params: {
                  level: getLogLevel(params.type),
                  source: realm.source,
                  text: getRemoteValuesText(args, true),
                  timestamp: Math.round(params.timestamp),
                  stackTrace: getBidiStackTrace(params.stackTrace),
                  type: 'console',
                  method: getLogMethod(params.type),
                  args,
                },
              },
            }),
            (error) => ({
              kind: 'error',
              error,
            })
          ),
          browsingContext.id,
          ChromiumBidi.Log.EventNames.LogEntryAdded
        );
      }
    });

    this.#cdpTarget.cdpClient.on('Runtime.exceptionThrown', (params) => {
      // Try to find realm by `cdpSessionId` and `executionContextId`,
      // if provided.
      const realm = this.#realmStorage.findRealm({
        cdpSessionId: this.#cdpTarget.cdpSessionId,
        executionContextId: params.exceptionDetails.executionContextId,
      });
      if (realm === undefined) {
        // Ignore exceptions not attached to any realm.
        this.#logger?.(LogType.cdp, params);
        return;
      }

      for (const browsingContext of realm.associatedBrowsingContexts) {
        this.#eventManager.registerPromiseEvent(
          LogManager.#getExceptionText(params, realm).then(
            (text) => ({
              kind: 'success',
              value: {
                type: 'event',
                method: ChromiumBidi.Log.EventNames.LogEntryAdded,
                params: {
                  level: Log.Level.Error,
                  source: realm.source,
                  text,
                  timestamp: Math.round(params.timestamp),
                  stackTrace: getBidiStackTrace(
                    params.exceptionDetails.stackTrace
                  ),
                  type: 'javascript',
                },
              },
            }),
            (error) => ({
              kind: 'error',
              error,
            })
          ),
          browsingContext.id,
          ChromiumBidi.Log.EventNames.LogEntryAdded
        );
      }
    });
  }

  /**
   * Try the best to get the exception text.
   */
  static async #getExceptionText(
    params: Protocol.Runtime.ExceptionThrownEvent,
    realm?: Realm
  ): Promise<string> {
    if (!params.exceptionDetails.exception) {
      return params.exceptionDetails.text;
    }
    if (realm === undefined) {
      return JSON.stringify(params.exceptionDetails.exception);
    }
    return await realm.stringifyObject(params.exceptionDetails.exception);
  }
}
