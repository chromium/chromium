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

import { Protocol } from 'devtools-protocol';
import { CdpClient } from '../../../cdp';
import { CommonDataTypes, Script } from '../protocol/bidiProtocolTypes';

export class ScriptEvaluator {
  #cdpClient: CdpClient;
  // As `script.evaluate` wraps call into serialization script, `lineNumber`
  // should be adjusted.
  readonly #evaluateStacktraceLineOffset = 0;
  readonly #callFunctionStacktraceLineOffset = 1;

  private constructor(_cdpClient: CdpClient) {
    this.#cdpClient = _cdpClient;
  }

  public static create(_cdpClient: CdpClient) {
    return new ScriptEvaluator(_cdpClient);
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   * @param cdpObject CDP remote object to be serialized.
   * @param resultOwnership indicates desired OwnershipModel.
   */
  public async serializeCdpObject(
    cdpObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.OwnershipModel
  ): Promise<CommonDataTypes.RemoteValue> {
    const cdpWebDriverValue: Protocol.Runtime.CallFunctionOnResponse =
      await this.#cdpClient.Runtime.callFunctionOn({
        functionDeclaration: String((obj: unknown) => obj),
        awaitPromise: true,
        arguments: [cdpObject],
        generateWebDriverValue: true,
        objectId: await this.#getDummyContextId(),
      });
    return await this.#cdpToBidiValue(cdpWebDriverValue, resultOwnership);
  }

  /**
   * Gets the string representation of an object. This is equivalent to
   * calling toString() on the object value.
   * @param cdpObject CDP remote object representing an object.
   * @returns string The stringified object.
   */
  async stringifyObject(
    cdpObject: Protocol.Runtime.RemoteObject
  ): Promise<string> {
    let stringifyResult = await this.#cdpClient.Runtime.callFunctionOn({
      functionDeclaration: String(function (
        obj: Protocol.Runtime.RemoteObject
      ) {
        return String(obj);
      }),
      awaitPromise: true,
      arguments: [cdpObject],
      returnByValue: true,
      objectId: await this.#getDummyContextId(),
    });
    return stringifyResult.result.value;
  }

  // TODO(sadym): `dummyContextObject` needed for the running context.
  // Use the proper `executionContextId` instead:
  // https://github.com/GoogleChromeLabs/chromium-bidi/issues/52
  async #getDummyContextId(): Promise<string> {
    const dummyContextObject = await this.#cdpClient.Runtime.evaluate({
      expression: '(()=>{return {}})()',
    });
    return dummyContextObject.result.objectId!;
  }

  public async callFunction(
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

    const thisAndArgumentsList = [await this.#deserializeToCdpArg(_this)];
    thisAndArgumentsList.push(
      ...(await Promise.all(
        _arguments.map(async (a) => {
          return await this.#deserializeToCdpArg(a);
        })
      ))
    );

    const cdpCallFunctionResult = await this.#cdpClient.Runtime.callFunctionOn({
      functionDeclaration: callFunctionAndSerializeScript,
      awaitPromise,
      arguments: thisAndArgumentsList, // this, arguments.
      generateWebDriverValue: true,
      objectId: await this.#getDummyContextId(),
    });

    if (cdpCallFunctionResult.exceptionDetails) {
      // Serialize exception details.
      return {
        exceptionDetails: await this.#serializeCdpExceptionDetails(
          cdpCallFunctionResult.exceptionDetails,
          this.#callFunctionStacktraceLineOffset,
          resultOwnership
        ),
        realm: 'TODO: ADD',
      };
    }

    return {
      result: await this.#cdpToBidiValue(
        cdpCallFunctionResult,
        resultOwnership
      ),
      realm: 'TODO: ADD',
    };
  }

  async #serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number,
    resultOwnership: Script.OwnershipModel
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
      resultOwnership
    );

    const text = await this.stringifyObject(cdpExceptionDetails.exception!);

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

  async #cdpToBidiValue(
    cdpValue:
      | Protocol.Runtime.CallFunctionOnResponse
      | Protocol.Runtime.EvaluateResponse,
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
    } else {
      await this.#cdpClient.Runtime.releaseObject({ objectId });
    }

    return bidiValue as CommonDataTypes.RemoteValue;
  }

  public async scriptEvaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.OwnershipModel
  ): Promise<Script.EvaluateResult> {
    let cdpEvaluateResult = await this.#cdpClient.Runtime.evaluate({
      expression,
      awaitPromise,
      generateWebDriverValue: true,
    });

    if (cdpEvaluateResult.exceptionDetails) {
      // Serialize exception details.
      return {
        result: {
          exceptionDetails: await this.#serializeCdpExceptionDetails(
            cdpEvaluateResult.exceptionDetails,
            this.#evaluateStacktraceLineOffset,
            resultOwnership
          ),
          realm: 'TODO: ADD',
        },
      };
    }

    return {
      result: {
        result: await this.#cdpToBidiValue(cdpEvaluateResult, resultOwnership),
        realm: 'TODO: ADD',
      },
    };
  }

  async #deserializeToCdpArg(
    argumentValue: Script.ArgumentValue
  ): Promise<Protocol.Runtime.CallArgument> {
    if ('handle' in argumentValue) {
      return { objectId: argumentValue.handle };
    }
    switch (argumentValue.type) {
      // Primitive Protocol Value
      // https://w3c.github.io/webdriver-bidi/#data-types-protocolValue-primitiveProtocolValue
      case 'undefined': {
        return { unserializableValue: 'undefined' };
      }
      case 'null': {
        return { unserializableValue: 'null' };
      }
      case 'string': {
        return { value: argumentValue.value };
      }
      case 'number': {
        if (argumentValue.value === 'NaN') {
          return { unserializableValue: 'NaN' };
        } else if (argumentValue.value === '-0') {
          return { unserializableValue: '-0' };
        } else if (argumentValue.value === '+Infinity') {
          return { unserializableValue: '+Infinity' };
        } else if (argumentValue.value === 'Infinity') {
          return { unserializableValue: 'Infinity' };
        } else if (argumentValue.value === '-Infinity') {
          return { unserializableValue: '-Infinity' };
        } else {
          return {
            value: argumentValue.value,
          };
        }
      }
      case 'boolean': {
        return { value: !!argumentValue.value };
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
          argumentValue.value
        );
        let argEvalResult = await this.#cdpClient.Runtime.callFunctionOn({
          functionDeclaration: String(function (
            ...args: Protocol.Runtime.CallArgument[]
          ) {
            const result = new Map();
            for (let i = 0; i < args.length; i += 2) {
              result.set(args[i], args[i + 1]);
            }
            return result;
          }),
          awaitPromise: true,
          arguments: keyValueArray,
          returnByValue: false,
          objectId: await this.#getDummyContextId(),
        });

        // TODO(sadym): dispose nested objects.

        return { objectId: argEvalResult.result.objectId };
      }
      case 'object': {
        // TODO(sadym): if non of the nested keys and values has remote
        //  reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          argumentValue.value
        );

        let argEvalResult = await this.#cdpClient.Runtime.callFunctionOn({
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
              result[key] = args[i + 1];
            }
            return result;
          }),
          awaitPromise: true,
          arguments: keyValueArray,
          returnByValue: false,
          objectId: await this.#getDummyContextId(),
        });

        // TODO(sadym): dispose nested objects.

        return { objectId: argEvalResult.result.objectId };
      }
      case 'array': {
        // TODO(sadym): if non of the nested items has remote reference,
        //  serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(argumentValue.value);

        let argEvalResult = await this.#cdpClient.Runtime.callFunctionOn({
          functionDeclaration: String(function (...args: unknown[]) {
            return args;
          }),
          awaitPromise: true,
          arguments: args,
          returnByValue: false,
          objectId: await this.#getDummyContextId(),
        });

        // TODO(sadym): dispose nested objects.

        return { objectId: argEvalResult.result.objectId };
      }
      case 'set': {
        // TODO(sadym): if non of the nested items has remote reference,
        //  serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(argumentValue.value);

        let argEvalResult = await this.#cdpClient.Runtime.callFunctionOn({
          functionDeclaration: String(function (...args: unknown[]) {
            return new Set(args);
          }),
          awaitPromise: true,
          arguments: args,
          returnByValue: false,
          objectId: await this.#getDummyContextId(),
        });
        return { objectId: argEvalResult.result.objectId };
      }

      // TODO(sadym): dispose nested objects.

      default:
        throw new Error(
          `Value ${JSON.stringify(argumentValue)} is not deserializable.`
        );
    }
  }

  async #flattenKeyValuePairs(
    value: CommonDataTypes.MappingLocalValue
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const keyValueArray: Protocol.Runtime.CallArgument[] = [];
    for (let pair of value) {
      const key = pair[0];
      const value = pair[1];

      let keyArg, valueArg;

      if (typeof key === 'string') {
        // Key is a string.
        keyArg = { value: key };
      } else {
        // Key is a serialized value.
        keyArg = await this.#deserializeToCdpArg(key);
      }

      valueArg = await this.#deserializeToCdpArg(value);

      keyValueArray.push(keyArg);
      keyValueArray.push(valueArg);
    }
    return keyValueArray;
  }

  async #flattenValueList(
    list: CommonDataTypes.ListLocalValue
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const result: Protocol.Runtime.CallArgument[] = [];

    for (let value of list) {
      result.push(await this.#deserializeToCdpArg(value));
    }

    return result;
  }
}
