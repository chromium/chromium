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

import {Realm} from './realm.js';

// As `script.evaluate` wraps call into serialization script, `lineNumber`
// should be adjusted.
const CALL_FUNCTION_STACKTRACE_LINE_OFFSET = 1;
const EVALUATE_STACKTRACE_LINE_OFFSET = 0;
export const SHARED_ID_DIVIDER = '_element_';

function cdpRemoteObjectToCallArgument(
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

async function deserializeToCdpArg(
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
      // TODO: release `obj.object.objectId` after using.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/375
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
    case 'undefined': {
      return {unserializableValue: 'undefined'};
    }
    case 'null': {
      return {unserializableValue: 'null'};
    }
    case 'string': {
      return {value: argumentValue.value};
    }
    case 'number': {
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
    }
    case 'boolean': {
      return {value: Boolean(argumentValue.value)};
    }
    case 'bigint': {
      return {
        unserializableValue: `BigInt(${JSON.stringify(argumentValue.value)})`,
      };
    }

    // Local Value
    // https://w3c.github.io/webdriver-bidi/#data-types-protocolValue-LocalValue
    case 'date': {
      return {
        unserializableValue: `new Date(Date.parse(${JSON.stringify(
          argumentValue.value
        )}))`,
      };
    }
    case 'regexp': {
      return {
        unserializableValue: `new RegExp(${JSON.stringify(
          argumentValue.value.pattern
        )}, ${JSON.stringify(argumentValue.value.flags)})`,
      };
    }
    case 'map': {
      // TODO(sadym): if non of the nested keys and values has remote
      // reference, serialize to `unserializableValue` without CDP roundtrip.
      const keyValueArray = await flattenKeyValuePairs(
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
      // TODO: release `argEvalResult.result.objectId`  after using.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/375
      return {objectId: argEvalResult.result.objectId};
    }
    case 'object': {
      // TODO(sadym): if non of the nested keys and values has remote
      //  reference, serialize to `unserializableValue` without CDP roundtrip.
      const keyValueArray = await flattenKeyValuePairs(
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
      // TODO: release `argEvalResult.result.objectId`  after using.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/375
      return {objectId: argEvalResult.result.objectId};
    }
    case 'array': {
      // TODO(sadym): if non of the nested items has remote reference,
      //  serialize to `unserializableValue` without CDP roundtrip.
      const args = await flattenValueList(argumentValue.value, realm);

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
      // TODO: release `argEvalResult.result.objectId`  after using.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/375
      return {objectId: argEvalResult.result.objectId};
    }
    case 'set': {
      // TODO(sadym): if non of the nested items has remote reference,
      //  serialize to `unserializableValue` without CDP roundtrip.
      const args = await flattenValueList(argumentValue.value, realm);

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
      // TODO: release `argEvalResult.result.objectId`  after using.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/375
      return {objectId: argEvalResult.result.objectId};
    }

    // TODO(sadym): dispose nested objects.

    default:
      throw new Error(
        `Value ${JSON.stringify(argumentValue)} is not deserializable.`
      );
  }
}

async function flattenKeyValuePairs(
  value: CommonDataTypes.MappingLocalValue,
  realm: Realm
): Promise<Protocol.Runtime.CallArgument[]> {
  const keyValueArray: Protocol.Runtime.CallArgument[] = [];
  for (const pair of value) {
    const key = pair[0];
    const value = pair[1];

    let keyArg;
    if (typeof key === 'string') {
      // Key is a string.
      keyArg = {value: key};
    } else {
      // Key is a serialized value.
      keyArg = await deserializeToCdpArg(key, realm);
    }

    const valueArg = await deserializeToCdpArg(value, realm);

    keyValueArray.push(keyArg);
    keyValueArray.push(valueArg);
  }
  return keyValueArray;
}

async function flattenValueList(
  list: CommonDataTypes.ListLocalValue,
  realm: Realm
): Promise<Protocol.Runtime.CallArgument[]> {
  const result: Protocol.Runtime.CallArgument[] = [];

  for (const value of list) {
    result.push(await deserializeToCdpArg(value, realm));
  }

  return result;
}

/**
 * Gets the string representation of an object. This is equivalent to
 * calling toString() on the object value.
 * @param cdpObject CDP remote object representing an object.
 * @param realm
 * @return string The stringified object.
 */
export async function stringifyObject(
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

export class ScriptEvaluator {
  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpRemoteObject CDP remote object to be serialized.
   * @param resultOwnership indicates desired OwnershipModel.
   * @param realm
   */
  async serializeCdpObject(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.OwnershipModel,
    realm: Realm
  ): Promise<CommonDataTypes.RemoteValue> {
    const arg = cdpRemoteObjectToCallArgument(cdpRemoteObject);

    const cdpWebDriverValue: Protocol.Runtime.CallFunctionOnResponse =
      await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
        functionDeclaration: String((obj: unknown) => obj),
        awaitPromise: false,
        arguments: [arg],
        generateWebDriverValue: true,
        executionContextId: realm.executionContextId,
      });
    return realm.cdpToBidiValue(cdpWebDriverValue, resultOwnership);
  }

  async callFunction(
    realm: Realm,
    functionDeclaration: string,
    _this: Script.ArgumentValue,
    _arguments: Script.ArgumentValue[],
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.ScriptResult> {
    const callFunctionAndSerializeScript = `(...args)=>{ return _callFunction((\n${functionDeclaration}\n), args);
      function _callFunction(f, args) {
        const deserializedThis = args.shift();
        const deserializedArgs = args;
        return f.apply(deserializedThis, deserializedArgs);
      }}`;

    const thisAndArgumentsList = [await deserializeToCdpArg(_this, realm)];
    thisAndArgumentsList.push(
      ...(await Promise.all(
        _arguments.map(async (a) => {
          return deserializeToCdpArg(a, realm);
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
          generateWebDriverValue: true,
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
        ].includes(e.message)
      ) {
        throw new Message.InvalidArgumentException('Handle was not found.');
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
      result: await realm.cdpToBidiValue(
        cdpCallFunctionResult,
        resultOwnership
      ),
      realm: realm.realmId,
    };
  }

  async #serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number,
    resultOwnership: Script.OwnershipModel,
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

    const text = await stringifyObject(cdpExceptionDetails.exception!, realm);

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

  async scriptEvaluate(
    realm: Realm,
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.ScriptResult> {
    const cdpEvaluateResult = await realm.cdpClient.sendCommand(
      'Runtime.evaluate',
      {
        contextId: realm.executionContextId,
        expression,
        awaitPromise,
        generateWebDriverValue: true,
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
      result: await realm.cdpToBidiValue(cdpEvaluateResult, resultOwnership),
      realm: realm.realmId,
    };
  }
}
