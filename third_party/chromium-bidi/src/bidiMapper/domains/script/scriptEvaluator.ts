/**
 * Copyright 2022 Google LLC.
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
import {Protocol} from 'devtools-protocol';

import {CommonDataTypes, Message, Script} from '../../../protocol/protocol.js';
import {IEventManager} from '../events/EventManager.js';

import {Realm} from './realm.js';

// As `script.evaluate` wraps call into serialization script, `lineNumber`
// should be adjusted.
const CALL_FUNCTION_STACKTRACE_LINE_OFFSET = 1;
const EVALUATE_STACKTRACE_LINE_OFFSET = 0;
export const SHARED_ID_DIVIDER = '_element_';

export class ScriptEvaluator {
  readonly #eventManager: IEventManager;

  constructor(eventManager: IEventManager) {
    this.#eventManager = eventManager;
  }

  /**
   * Gets the string representation of an object. This is equivalent to
   * calling toString() on the object value.
   * @param cdpObject CDP remote object representing an object.
   * @param realm
   * @return string The stringified object.
   */
  static async stringifyObject(
    cdpObject: Protocol.Runtime.RemoteObject,
    realm: Realm
  ): Promise<string> {
    const stringifyResult = await realm.cdpClient.sendCommand(
      'Runtime.callFunctionOn',
      {
        functionDeclaration: String((obj: Protocol.Runtime.RemoteObject) => {
          return String(obj);
        }),
        awaitPromise: false,
        arguments: [cdpObject],
        returnByValue: true,
        executionContextId: realm.executionContextId,
      }
    );
    return stringifyResult.result.value;
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpRemoteObject CDP remote object to be serialized.
   * @param resultOwnership Indicates desired ResultOwnership.
   * @param realm
   */
  async serializeCdpObject(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.ResultOwnership,
    realm: Realm
  ): Promise<CommonDataTypes.RemoteValue> {
    const arg = ScriptEvaluator.#cdpRemoteObjectToCallArgument(cdpRemoteObject);

    const cdpWebDriverValue: Protocol.Runtime.CallFunctionOnResponse =
      await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
        functionDeclaration: String((obj: unknown) => obj),
        awaitPromise: false,
        arguments: [arg],
        serializationOptions: {
          serialization: 'deep',
        },
        executionContextId: realm.executionContextId,
      });
    return realm.cdpToBidiValue(cdpWebDriverValue, resultOwnership);
  }

  async scriptEvaluate(
    realm: Realm,
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.ResultOwnership,
    serializationOptions: Script.SerializationOptions
  ): Promise<Script.ScriptResult> {
    if (![0, undefined].includes(serializationOptions?.maxDomDepth))
      throw new Error('serializationOptions.maxDomDepth!=0 is not supported');

    const cdpEvaluateResult = await realm.cdpClient.sendCommand(
      'Runtime.evaluate',
      {
        contextId: realm.executionContextId,
        expression,
        awaitPromise,
        serializationOptions: {
          serialization: 'deep',
          ...(serializationOptions?.maxObjectDepth
            ? {}
            : {maxDepth: serializationOptions?.maxObjectDepth}),
        },
      }
    );

    if (cdpEvaluateResult.exceptionDetails) {
      // Serialize exception details.
      return {
        exceptionDetails: await this.#serializeCdpExceptionDetails(
          cdpEvaluateResult.exceptionDetails,
          EVALUATE_STACKTRACE_LINE_OFFSET,
          resultOwnership,
          realm
        ),
        type: 'exception',
        realm: realm.realmId,
      };
    }

    return {
      type: 'success',
      result: realm.cdpToBidiValue(cdpEvaluateResult, resultOwnership),
      realm: realm.realmId,
    };
  }

  async callFunction(
    realm: Realm,
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.ResultOwnership,
    serializationOptions: Script.SerializationOptions
  ): Promise<Script.ScriptResult> {
    if (![0, undefined].includes(serializationOptions?.maxDomDepth))
      throw new Error('serializationOptions.maxDomDepth!=0 is not supported');

    const callFunctionAndSerializeScript = `(...args)=>{ return _callFunction((\n${functionDeclaration}\n), args);
      function _callFunction(f, args) {
        const deserializedThis = args.shift();
        const deserializedArgs = args;
        return f.apply(deserializedThis, deserializedArgs);
      }}`;

    const thisAndArgumentsList = [
      await this.#deserializeToCdpArg(_this, realm),
    ];
    thisAndArgumentsList.push(
      ...(await Promise.all(
        _arguments.map(async (a) => {
          return this.#deserializeToCdpArg(a, realm);
        })
      ))
    );

    let cdpCallFunctionResult: Protocol.Runtime.CallFunctionOnResponse;
    try {
      cdpCallFunctionResult = await realm.cdpClient.sendCommand(
        'Runtime.callFunctionOn',
        {
          functionDeclaration: callFunctionAndSerializeScript,
          awaitPromise,
          arguments: thisAndArgumentsList, // this, arguments.
          serializationOptions: {
            serialization: 'deep',
            ...(serializationOptions?.maxObjectDepth
              ? {}
              : {maxDepth: serializationOptions?.maxObjectDepth}),
          },
          executionContextId: realm.executionContextId,
        }
      );
    } catch (e: any) {
      // Heuristic to determine if the problem is in the argument.
      // The check can be done on the `deserialization` step, but this approach
      // helps to save round-trips.
      if (
        e.code === -32000 &&
        [
          'Could not find object with given id',
          'Argument should belong to the same JavaScript world as target object',
          'Invalid remote object id',
        ].includes(e.message)
      ) {
        throw new Message.NoSuchHandleException('Handle was not found.');
      }
      throw e;
    }

    if (cdpCallFunctionResult.exceptionDetails) {
      // Serialize exception details.
      return {
        exceptionDetails: await this.#serializeCdpExceptionDetails(
          cdpCallFunctionResult.exceptionDetails,
          CALL_FUNCTION_STACKTRACE_LINE_OFFSET,
          resultOwnership,
          realm
        ),
        type: 'exception',
        realm: realm.realmId,
      };
    }
    return {
      type: 'success',
      result: realm.cdpToBidiValue(cdpCallFunctionResult, resultOwnership),
      realm: realm.realmId,
    };
  }

  static #cdpRemoteObjectToCallArgument(
    cdpRemoteObject: Protocol.Runtime.RemoteObject
  ): Protocol.Runtime.CallArgument {
    if (cdpRemoteObject.objectId !== undefined) {
      return {objectId: cdpRemoteObject.objectId};
    }
    if (cdpRemoteObject.unserializableValue !== undefined) {
      return {unserializableValue: cdpRemoteObject.unserializableValue};
    }
    return {value: cdpRemoteObject.value};
  }

  async #deserializeToCdpArg(
    argumentValue: Script.ArgumentValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument> {
    if ('sharedId' in argumentValue) {
      const [navigableId, rawBackendNodeId] =
        argumentValue.sharedId.split(SHARED_ID_DIVIDER);

      const backendNodeId = parseInt(rawBackendNodeId ?? '');
      if (
        isNaN(backendNodeId) ||
        backendNodeId === undefined ||
        navigableId === undefined
      ) {
        throw new Message.InvalidArgumentException(
          `SharedId "${argumentValue.sharedId}" should have format "{navigableId}${SHARED_ID_DIVIDER}{backendNodeId}".`
        );
      }

      if (realm.navigableId !== navigableId) {
        throw new Message.NoSuchNodeException(
          `SharedId "${argumentValue.sharedId}" belongs to different document. Current document is ${realm.navigableId}.`
        );
      }

      try {
        const obj = await realm.cdpClient.sendCommand('DOM.resolveNode', {
          backendNodeId,
          executionContextId: realm.executionContextId,
        });
        // TODO(#375): Release `obj.object.objectId` after using.
        return {objectId: obj.object.objectId};
      } catch (e: any) {
        // Heuristic to detect "no such node" exception. Based on the  specific
        // CDP implementation.
        if (e.code === -32000 && e.message === 'No node with given id found') {
          throw new Message.NoSuchNodeException(
            `SharedId "${argumentValue.sharedId}" was not found.`
          );
        }
        throw e;
      }
    }
    if ('handle' in argumentValue) {
      return {objectId: argumentValue.handle};
    }
    switch (argumentValue.type) {
      // Primitive Protocol Value
      // https://w3c.github.io/webdriver-bidi/#data-types-protocolValue-primitiveProtocolValue
      case 'undefined':
        return {unserializableValue: 'undefined'};
      case 'null':
        return {unserializableValue: 'null'};
      case 'string':
        return {value: argumentValue.value};
      case 'number':
        if (argumentValue.value === 'NaN') {
          return {unserializableValue: 'NaN'};
        } else if (argumentValue.value === '-0') {
          return {unserializableValue: '-0'};
        } else if (argumentValue.value === 'Infinity') {
          return {unserializableValue: 'Infinity'};
        } else if (argumentValue.value === '-Infinity') {
          return {unserializableValue: '-Infinity'};
        }
        return {
          value: argumentValue.value,
        };
      case 'boolean':
        return {value: Boolean(argumentValue.value)};
      case 'bigint':
        return {
          unserializableValue: `BigInt(${JSON.stringify(argumentValue.value)})`,
        };
      case 'date':
        return {
          unserializableValue: `new Date(Date.parse(${JSON.stringify(
            argumentValue.value
          )}))`,
        };
      case 'regexp':
        return {
          unserializableValue: `new RegExp(${JSON.stringify(
            argumentValue.value.pattern
          )}, ${JSON.stringify(argumentValue.value.flags)})`,
        };
      case 'map': {
        // TODO: If none of the nested keys and values has a remote
        // reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          argumentValue.value,
          realm
        );
        const argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (...args: Protocol.Runtime.CallArgument[]) => {
                const result = new Map();
                for (let i = 0; i < args.length; i += 2) {
                  result.set(args[i], args[i + 1]);
                }
                return result;
              }
            ),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );
        // TODO(#375): Release `argEvalResult.result.objectId` after using.
        return {objectId: argEvalResult.result.objectId};
      }
      case 'object': {
        // TODO: If none of the nested keys and values has a remote
        //  reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          argumentValue.value,
          realm
        );

        const argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (...args: Protocol.Runtime.CallArgument[]) => {
                const result: Record<
                  string | number | symbol,
                  Protocol.Runtime.CallArgument
                > = {};

                for (let i = 0; i < args.length; i += 2) {
                  // Key should be either `string`, `number`, or `symbol`.
                  const key = args[i] as string | number | symbol;
                  result[key] = args[i + 1]!;
                }
                return result;
              }
            ),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );
        // TODO(#375): Release `argEvalResult.result.objectId` after using.
        return {objectId: argEvalResult.result.objectId};
      }
      case 'array': {
        // TODO: If none of the nested items has a remote reference,
        // serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(argumentValue.value, realm);

        const argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String((...args: unknown[]) => {
              return args;
            }),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );
        // TODO(#375): Release `argEvalResult.result.objectId` after using.
        return {objectId: argEvalResult.result.objectId};
      }
      case 'set': {
        // TODO: if none of the nested items has a remote reference,
        // serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(argumentValue.value, realm);

        const argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String((...args: unknown[]) => {
              return new Set(args);
            }),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );
        // TODO(#375): Release `argEvalResult.result.objectId` after using.
        return {objectId: argEvalResult.result.objectId};
      }

      case 'channel': {
        const createChannelHandleResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(() => {
              const queue: unknown[] = [];
              let queueNonEmptyResolver: null | (() => void) = null;

              return {
                /**
                 * Gets a promise, which is resolved as soon as a message occurs
                 * in the queue.
                 */
                async getMessage(): Promise<unknown> {
                  const onMessage =
                    queue.length > 0
                      ? Promise.resolve()
                      : new Promise<void>((resolve) => {
                          queueNonEmptyResolver = resolve;
                        });
                  await onMessage;
                  return queue.shift();
                },

                /**
                 * Adds a message to the queue.
                 * Resolves the pending promise if needed.
                 */
                sendMessage(message: string) {
                  queue.push(message);
                  if (queueNonEmptyResolver !== null) {
                    queueNonEmptyResolver();
                    queueNonEmptyResolver = null;
                  }
                },
              };
            }),
            returnByValue: false,
            executionContextId: realm.executionContextId,
            serializationOptions: {
              serialization: 'deep',
            },
          }
        );
        const channelHandle = createChannelHandleResult.result.objectId;

        // Long-poll the message queue asynchronously.
        void this.#initChannelListener(argumentValue, channelHandle, realm);

        const sendMessageArgResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (channelHandle: {sendMessage: (message: string) => void}) => {
                return channelHandle.sendMessage;
              }
            ),
            arguments: [
              {
                objectId: channelHandle,
              },
            ],
            returnByValue: false,
            executionContextId: realm.executionContextId,
            serializationOptions: {
              serialization: 'deep',
            },
          }
        );
        return {objectId: sendMessageArgResult.result.objectId};
      }

      // TODO(#375): Dispose of nested objects.

      default:
        throw new Error(
          `Value ${JSON.stringify(argumentValue)} is not deserializable.`
        );
    }
  }

  async #flattenKeyValuePairs(
    mapping: CommonDataTypes.MappingLocalValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const keyValueArray: Protocol.Runtime.CallArgument[] = [];
    for (const [key, value] of mapping) {
      let keyArg;
      if (typeof key === 'string') {
        // Key is a string.
        keyArg = {value: key};
      } else {
        // Key is a serialized value.
        keyArg = await this.#deserializeToCdpArg(key, realm);
      }

      const valueArg = await this.#deserializeToCdpArg(value, realm);

      keyValueArray.push(keyArg);
      keyValueArray.push(valueArg);
    }
    return keyValueArray;
  }

  async #flattenValueList(
    list: CommonDataTypes.ListLocalValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument[]> {
    return Promise.all(
      list.map((value) => this.#deserializeToCdpArg(value, realm))
    );
  }

  async #initChannelListener(
    channel: Script.ChannelValue,
    channelHandle: string | undefined,
    realm: Realm
  ) {
    const channelId = channel.value.channel;

    // TODO(#294): Remove this loop after the realm is destroyed.
    // Rely on the CDP throwing exception in such a case.
    for (;;) {
      const message = await realm.cdpClient.sendCommand(
        'Runtime.callFunctionOn',
        {
          functionDeclaration: String(
            async (channelHandle: {getMessage: () => Promise<unknown>}) =>
              channelHandle.getMessage()
          ),
          arguments: [
            {
              objectId: channelHandle,
            },
          ],
          awaitPromise: true,
          executionContextId: realm.executionContextId,
          serializationOptions: {
            serialization: 'deep',
          },
        }
      );

      this.#eventManager.registerEvent(
        {
          method: Script.EventNames.MessageEvent,
          params: {
            channel: channelId,
            data: realm.cdpToBidiValue(
              message,
              channel.value.ownership ?? 'none'
            ),
            source: {
              realm: realm.realmId,
              context: realm.browsingContextId,
            },
          },
        },
        realm.browsingContextId
      );
    }
  }

  async #serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number,
    resultOwnership: Script.ResultOwnership,
    realm: Realm
  ): Promise<Script.ExceptionDetails> {
    const callFrames = cdpExceptionDetails.stackTrace?.callFrames.map(
      (frame) => ({
        url: frame.url,
        functionName: frame.functionName,
        // As `script.evaluate` wraps call into serialization script, so
        // `lineNumber` should be adjusted.
        lineNumber: frame.lineNumber - lineOffset,
        columnNumber: frame.columnNumber,
      })
    );

    const exception = await this.serializeCdpObject(
      // Exception should always be there.
      cdpExceptionDetails.exception!,
      resultOwnership,
      realm
    );

    const text = await ScriptEvaluator.stringifyObject(
      cdpExceptionDetails.exception!,
      realm
    );

    return {
      exception,
      columnNumber: cdpExceptionDetails.columnNumber,
      // As `script.evaluate` wraps call into serialization script, so
      // `lineNumber` should be adjusted.
      lineNumber: cdpExceptionDetails.lineNumber - lineOffset,
      stackTrace: {
        callFrames: callFrames || [],
      },
      text: text || cdpExceptionDetails.text,
    };
  }
}
