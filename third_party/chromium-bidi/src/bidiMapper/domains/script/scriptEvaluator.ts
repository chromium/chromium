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
import {CommonDataTypes, Script, Message} from '../../../protocol/protocol';
import {Realm} from './realm';

export class ScriptEvaluator {
  // As `script.evaluate` wraps call into serialization script, `lineNumber`
  // should be adjusted.
  static readonly #evaluateStacktraceLineOffset = 0;
  static readonly #callFunctionStacktraceLineOffset = 1;

  // Keeps track of `handle`s and their realms sent to client.
  static readonly #knownHandlesToRealm: Map<string, string> = new Map();

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpRemoteObject CDP remote object to be serialized.
   * @param resultOwnership indicates desired OwnershipModel.
   * @param realm
   */
  public static async serializeCdpObject(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.OwnershipModel,
    realm: Realm
  ): Promise<CommonDataTypes.RemoteValue> {
    const arg = this.#cdpRemoteObjectToCallArgument(cdpRemoteObject);

    const cdpWebDriverValue: Protocol.Runtime.CallFunctionOnResponse =
      await realm.cdpClient.sendCommand('Runtime.callFunctionOn', {
        functionDeclaration: String((obj: unknown) => obj),
        awaitPromise: false,
        arguments: [arg],
        generateWebDriverValue: true,
        executionContextId: realm.executionContextId,
      });
    return await this.#cdpToBidiValue(
      cdpWebDriverValue,
      realm,
      resultOwnership
    );
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

  /**
   * Gets the string representation of an object. This is equivalent to
   * calling toString() on the object value.
   * @param cdpObject CDP remote object representing an object.
   * @param realm
   * @returns string The stringified object.
   */
  public static async stringifyObject(
    cdpObject: Protocol.Runtime.RemoteObject,
    realm: Realm
  ): Promise<string> {
    let stringifyResult = await realm.cdpClient.sendCommand(
      'Runtime.callFunctionOn',
      {
        functionDeclaration: String(function (
          obj: Protocol.Runtime.RemoteObject
        ) {
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

  public static async callFunction(
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

    const thisAndArgumentsList = [
      await this.#deserializeToCdpArg(_this, realm),
    ];
    thisAndArgumentsList.push(
      ...(await Promise.all(
        _arguments.map(async (a) => {
          return await this.#deserializeToCdpArg(a, realm);
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
          this.#callFunctionStacktraceLineOffset,
          resultOwnership,
          realm
        ),
        realm: realm.realmId,
      };
    }
    return {
      result: await ScriptEvaluator.#cdpToBidiValue(
        cdpCallFunctionResult,
        realm,
        resultOwnership
      ),
      realm: realm.realmId,
    };
  }

  static realmDestroyed(realm: Realm) {
    return Array.from(this.#knownHandlesToRealm.entries())
      .filter(([, r]) => r === realm.realmId)
      .map(([h]) => this.#knownHandlesToRealm.delete(h));
  }

  static async disown(realm: Realm, handle: string) {
    // Disowning an object from different realm does nothing.
    if (ScriptEvaluator.#knownHandlesToRealm.get(handle) !== realm.realmId) {
      return;
    }
    try {
      await realm.cdpClient.sendCommand('Runtime.releaseObject', {
        objectId: handle,
      });
    } catch (e: any) {
      // Heuristic to determine if the problem is in the unknown handler.
      // Ignore the error if so.
      if (!(e.code === -32000 && e.message === 'Invalid remote object id')) {
        throw e;
      }
    }
    this.#knownHandlesToRealm.delete(handle);
  }

  static async #serializeCdpExceptionDetails(
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

    const text = await this.stringifyObject(
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

  static async #cdpToBidiValue(
    cdpValue:
      | Protocol.Runtime.CallFunctionOnResponse
      | Protocol.Runtime.EvaluateResponse,
    realm: Realm,
    resultOwnership: Script.OwnershipModel
  ): Promise<CommonDataTypes.RemoteValue> {
    // This relies on the CDP to implement proper BiDi serialization, except
    // objectIds+handles.
    const cdpWebDriverValue = cdpValue.result.webDriverValue!;
    if (!cdpValue.result.objectId) {
      return cdpWebDriverValue as CommonDataTypes.RemoteValue;
    }

    const objectId = cdpValue.result.objectId;
    const bidiValue = cdpWebDriverValue as any;

    if (resultOwnership === 'root') {
      bidiValue.handle = objectId;
      // Remember all the handles sent to client.
      this.#knownHandlesToRealm.set(objectId, realm.realmId);
    } else {
      await realm.cdpClient.sendCommand('Runtime.releaseObject', {objectId});
    }

    return bidiValue as CommonDataTypes.RemoteValue;
  }

  public static async scriptEvaluate(
    realm: Realm,
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.ScriptResult> {
    let cdpEvaluateResult = await realm.cdpClient.sendCommand(
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
          this.#evaluateStacktraceLineOffset,
          resultOwnership,
          realm
        ),
        realm: realm.realmId,
      };
    }

    return {
      result: await ScriptEvaluator.#cdpToBidiValue(
        cdpEvaluateResult,
        realm,
        resultOwnership
      ),
      realm: realm.realmId,
    };
  }

  static async #deserializeToCdpArg(
    argumentValue: Script.ArgumentValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument> {
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
        } else if (argumentValue.value === '+Infinity') {
          return {unserializableValue: '+Infinity'};
        } else if (argumentValue.value === 'Infinity') {
          return {unserializableValue: 'Infinity'};
        } else if (argumentValue.value === '-Infinity') {
          return {unserializableValue: '-Infinity'};
        } else {
          return {
            value: argumentValue.value,
          };
        }
      }
      case 'boolean': {
        return {value: !!argumentValue.value};
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
        //  reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          argumentValue.value,
          realm
        );
        let argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(function (
              ...args: Protocol.Runtime.CallArgument[]
            ) {
              const result = new Map();
              for (let i = 0; i < args.length; i += 2) {
                result.set(args[i], args[i + 1]);
              }
              return result;
            }),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );

        // TODO(sadym): dispose nested objects.

        return {objectId: argEvalResult.result.objectId};
      }
      case 'object': {
        // TODO(sadym): if non of the nested keys and values has remote
        //  reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          argumentValue.value,
          realm
        );

        let argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(function (
              ...args: Protocol.Runtime.CallArgument[]
            ) {
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
            }),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );

        // TODO(sadym): dispose nested objects.

        return {objectId: argEvalResult.result.objectId};
      }
      case 'array': {
        // TODO(sadym): if non of the nested items has remote reference,
        //  serialize to `unserializableValue` without CDP roundtrip.
        const args = await ScriptEvaluator.#flattenValueList(
          argumentValue.value,
          realm
        );

        let argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(function (...args: unknown[]) {
              return args;
            }),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );

        // TODO(sadym): dispose nested objects.

        return {objectId: argEvalResult.result.objectId};
      }
      case 'set': {
        // TODO(sadym): if non of the nested items has remote reference,
        //  serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(argumentValue.value, realm);

        let argEvalResult = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(function (...args: unknown[]) {
              return new Set(args);
            }),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: realm.executionContextId,
          }
        );
        return {objectId: argEvalResult.result.objectId};
      }

      // TODO(sadym): dispose nested objects.

      default:
        throw new Error(
          `Value ${JSON.stringify(argumentValue)} is not deserializable.`
        );
    }
  }

  static async #flattenKeyValuePairs(
    value: CommonDataTypes.MappingLocalValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const keyValueArray: Protocol.Runtime.CallArgument[] = [];
    for (let pair of value) {
      const key = pair[0];
      const value = pair[1];

      let keyArg, valueArg;

      if (typeof key === 'string') {
        // Key is a string.
        keyArg = {value: key};
      } else {
        // Key is a serialized value.
        keyArg = await this.#deserializeToCdpArg(key, realm);
      }

      valueArg = await this.#deserializeToCdpArg(value, realm);

      keyValueArray.push(keyArg);
      keyValueArray.push(valueArg);
    }
    return keyValueArray;
  }

  static async #flattenValueList(
    list: CommonDataTypes.ListLocalValue,
    realm: Realm
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const result: Protocol.Runtime.CallArgument[] = [];

    for (let value of list) {
      result.push(await this.#deserializeToCdpArg(value, realm));
    }

    return result;
  }
}
