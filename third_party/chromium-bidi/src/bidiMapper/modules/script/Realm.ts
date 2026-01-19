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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  ChromiumBidi,
  NoSuchHandleException,
  Script,
} from '../../../protocol/protocol.js';
import {CdpErrorConstants} from '../../../utils/cdpErrorConstants.js';
import {LogType, type LoggerFn} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import type {EventManager} from '../session/EventManager.js';

import {ChannelProxy} from './ChannelProxy.js';
import type {RealmStorage} from './RealmStorage.js';

export abstract class Realm {
  readonly #cdpClient: CdpClient;
  readonly #eventManager: EventManager;
  readonly #executionContextId: Protocol.Runtime.ExecutionContextId;
  readonly #logger?: LoggerFn;
  readonly #origin: string;
  readonly #realmId: Script.Realm;
  protected realmStorage: RealmStorage;

  constructor(
    cdpClient: CdpClient,
    eventManager: EventManager,
    executionContextId: Protocol.Runtime.ExecutionContextId,
    logger: LoggerFn | undefined,
    origin: string,
    realmId: Script.Realm,
    realmStorage: RealmStorage,
  ) {
    this.#cdpClient = cdpClient;
    this.#eventManager = eventManager;
    this.#executionContextId = executionContextId;
    this.#logger = logger;
    this.#origin = origin;
    this.#realmId = realmId;
    this.realmStorage = realmStorage;

    this.realmStorage.addRealm(this);
  }

  cdpToBidiValue(
    cdpValue:
      | Protocol.Runtime.CallFunctionOnResponse
      | Protocol.Runtime.EvaluateResponse,
    resultOwnership: Script.ResultOwnership,
  ): Script.RemoteValue {
    const bidiValue = this.serializeForBiDi(
      cdpValue.result.deepSerializedValue!,
      new Map(),
    );

    if (cdpValue.result.objectId) {
      const objectId = cdpValue.result.objectId;
      if (resultOwnership === Script.ResultOwnership.Root) {
        // Extend BiDi value with `handle` based on required `resultOwnership`
        // and  CDP response but not on the actual BiDi type.
        (bidiValue as any).handle = objectId;
        // Remember all the handles sent to client.
        this.realmStorage.knownHandlesToRealmMap.set(objectId, this.realmId);
      } else {
        // No need to await for the object to be released.
        void this.#releaseObject(objectId).catch((error) =>
          this.#logger?.(LogType.debugError, error),
        );
      }
    }

    return bidiValue;
  }

  isHidden(): boolean {
    return false;
  }

  /**
   * Relies on the CDP to implement proper BiDi serialization, except:
   * * CDP integer property `backendNodeId` is replaced with `sharedId` of
   * `{documentId}_element_{backendNodeId}`;
   * * CDP integer property `weakLocalObjectReference` is replaced with UUID `internalId`
   * using unique-per serialization `internalIdMap`.
   * * CDP type `platformobject` is replaced with `object`.
   * @param deepSerializedValue - CDP value to be converted to BiDi.
   * @param internalIdMap - Map from CDP integer `weakLocalObjectReference` to BiDi UUID
   * `internalId`.
   */
  protected serializeForBiDi(
    deepSerializedValue: Protocol.Runtime.DeepSerializedValue,
    internalIdMap: Map<number, string>,
  ): Script.RemoteValue {
    if (Object.hasOwn(deepSerializedValue, 'weakLocalObjectReference')) {
      const weakLocalObjectReference =
        deepSerializedValue.weakLocalObjectReference!;
      if (!internalIdMap.has(weakLocalObjectReference)) {
        internalIdMap.set(weakLocalObjectReference, uuidv4());
      }

      (
        deepSerializedValue as Protocol.Runtime.DeepSerializedValue & {
          internalId?: string;
        }
      ).internalId = internalIdMap.get(weakLocalObjectReference);
      delete deepSerializedValue['weakLocalObjectReference'];
    }

    if (
      deepSerializedValue.type === 'node' &&
      deepSerializedValue.value &&
      Object.hasOwn(deepSerializedValue.value, 'frameId')
    ) {
      // `frameId` is not needed in BiDi as it is not yet specified.
      delete deepSerializedValue.value['frameId'];
    }

    // Platform object is a special case. It should have only `{type: object}`
    // without `value` field.
    if ((deepSerializedValue.type as string) === 'platformobject') {
      return {type: 'object'};
    }

    const bidiValue = deepSerializedValue.value;
    if (bidiValue === undefined) {
      return deepSerializedValue as Script.RemoteValue;
    }

    // Recursively update the nested values.
    if (
      ['array', 'set', 'htmlcollection', 'nodelist'].includes(
        deepSerializedValue.type,
      )
    ) {
      for (const i in bidiValue) {
        bidiValue[i] = this.serializeForBiDi(bidiValue[i], internalIdMap);
      }
    }
    if (['object', 'map'].includes(deepSerializedValue.type)) {
      for (const i in bidiValue) {
        bidiValue[i] = [
          this.serializeForBiDi(bidiValue[i][0], internalIdMap),
          this.serializeForBiDi(bidiValue[i][1], internalIdMap),
        ];
      }
    }

    return deepSerializedValue as Script.RemoteValue;
  }

  get realmId(): Script.Realm {
    return this.#realmId;
  }

  get executionContextId(): Protocol.Runtime.ExecutionContextId {
    return this.#executionContextId;
  }

  get origin(): string {
    return this.#origin;
  }

  get source(): Script.Source {
    return {
      realm: this.realmId,
    };
  }

  get cdpClient(): CdpClient {
    return this.#cdpClient;
  }

  abstract get associatedBrowsingContexts(): BrowsingContextImpl[];

  abstract get realmType(): Script.RealmType;

  protected get baseInfo(): Script.BaseRealmInfo {
    return {
      realm: this.realmId,
      origin: this.origin,
    };
  }

  abstract get realmInfo(): Script.RealmInfo;

  async evaluate(
    expression: string,
    awaitPromise: boolean,
    resultOwnership: Script.ResultOwnership = Script.ResultOwnership.None,
    serializationOptions: Script.SerializationOptions = {},
    userActivation = false,
    includeCommandLineApi = false,
  ): Promise<Script.EvaluateResult> {
    const cdpEvaluateResult = await this.cdpClient.sendCommand(
      'Runtime.evaluate',
      {
        contextId: this.executionContextId,
        expression,
        awaitPromise,
        serializationOptions: Realm.#getSerializationOptions(
          Protocol.Runtime.SerializationOptionsSerialization.Deep,
          serializationOptions,
        ),
        userGesture: userActivation,
        includeCommandLineAPI: includeCommandLineApi,
      },
    );

    if (cdpEvaluateResult.exceptionDetails) {
      return await this.#getExceptionResult(
        cdpEvaluateResult.exceptionDetails,
        0,
        resultOwnership,
      );
    }

    return {
      realm: this.realmId,
      result: this.cdpToBidiValue(cdpEvaluateResult, resultOwnership),
      type: 'success',
    };
  }

  #registerEvent(event: ChromiumBidi.Event) {
    if (this.associatedBrowsingContexts.length === 0) {
      this.#eventManager.registerGlobalEvent(event);
    } else {
      for (const browsingContext of this.associatedBrowsingContexts) {
        this.#eventManager.registerEvent(event, browsingContext.id);
      }
    }
  }

  protected initialize() {
    if (!this.isHidden()) {
      // Report only not-hidden realms.
      this.#registerEvent({
        type: 'event',
        method: ChromiumBidi.Script.EventNames.RealmCreated,
        params: this.realmInfo,
      });
    }
  }

  /**
   * Serializes a given CDP object into BiDi, keeping references in the
   * target's `globalThis`.
   */
  async serializeCdpObject(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
    resultOwnership: Script.ResultOwnership,
  ): Promise<Script.RemoteValue> {
    // TODO: if the object is a primitive, return it directly without CDP roundtrip.
    const argument = Realm.#cdpRemoteObjectToCallArgument(cdpRemoteObject);

    const cdpValue: Protocol.Runtime.CallFunctionOnResponse =
      await this.cdpClient.sendCommand('Runtime.callFunctionOn', {
        functionDeclaration: String(
          (remoteObject: Protocol.Runtime.RemoteObject) => remoteObject,
        ),
        awaitPromise: false,
        arguments: [argument],
        serializationOptions: {
          serialization:
            Protocol.Runtime.SerializationOptionsSerialization.Deep,
        },
        executionContextId: this.executionContextId,
      });

    return this.cdpToBidiValue(cdpValue, resultOwnership);
  }

  static #cdpRemoteObjectToCallArgument(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
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
   * calling `toString()` on the object value.
   */
  async stringifyObject(
    cdpRemoteObject: Protocol.Runtime.RemoteObject,
  ): Promise<string> {
    const {result} = await this.cdpClient.sendCommand(
      'Runtime.callFunctionOn',
      {
        functionDeclaration: String(
          (remoteObject: Protocol.Runtime.RemoteObject) => String(remoteObject),
        ),
        awaitPromise: false,
        arguments: [cdpRemoteObject],
        returnByValue: true,
        executionContextId: this.executionContextId,
      },
    );
    return result.value;
  }

  async #flattenKeyValuePairs(
    mappingLocalValue: Script.MappingLocalValue,
  ): Promise<Protocol.Runtime.CallArgument[]> {
    const keyValueArray = await Promise.all(
      mappingLocalValue.map(async ([key, value]) => {
        let keyArg;
        if (typeof key === 'string') {
          // Key is a string.
          keyArg = {value: key};
        } else {
          // Key is a serialized value.
          keyArg = await this.deserializeForCdp(key);
        }
        const valueArg = await this.deserializeForCdp(value);

        return [keyArg, valueArg];
      }),
    );

    return keyValueArray.flat();
  }

  async #flattenValueList(
    listLocalValue: Script.ListLocalValue,
  ): Promise<Protocol.Runtime.CallArgument[]> {
    return await Promise.all(
      listLocalValue.map((localValue) => this.deserializeForCdp(localValue)),
    );
  }

  async #serializeCdpExceptionDetails(
    cdpExceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number,
    resultOwnership: Script.ResultOwnership,
  ): Promise<Script.ExceptionDetails> {
    const callFrames =
      cdpExceptionDetails.stackTrace?.callFrames.map((frame) => ({
        url: frame.url,
        functionName: frame.functionName,
        lineNumber: frame.lineNumber - lineOffset,
        columnNumber: frame.columnNumber,
      })) ?? [];

    // Exception should always be there.
    const exception = cdpExceptionDetails.exception!;

    return {
      exception: await this.serializeCdpObject(exception, resultOwnership),
      columnNumber: cdpExceptionDetails.columnNumber,
      lineNumber: cdpExceptionDetails.lineNumber - lineOffset,
      stackTrace: {
        callFrames,
      },
      text: (await this.stringifyObject(exception)) || cdpExceptionDetails.text,
    };
  }

  async callFunction(
    functionDeclaration: string,
    awaitPromise: boolean,
    thisLocalValue: Script.LocalValue = {
      type: 'undefined',
    },
    argumentsLocalValues: Script.LocalValue[] = [],
    resultOwnership: Script.ResultOwnership = Script.ResultOwnership.None,
    serializationOptions: Script.SerializationOptions = {},
    userActivation = false,
  ): Promise<Script.EvaluateResult> {
    const callFunctionAndSerializeScript = `(...args) => {
      function callFunction(f, args) {
        const deserializedThis = args.shift();
        const deserializedArgs = args;
        return f.apply(deserializedThis, deserializedArgs);
      }
      return callFunction((
        ${functionDeclaration}
      ), args);
    }`;

    const thisAndArgumentsList = [
      await this.deserializeForCdp(thisLocalValue),
      ...(await Promise.all(
        argumentsLocalValues.map(
          async (argumentLocalValue: Script.LocalValue) =>
            await this.deserializeForCdp(argumentLocalValue),
        ),
      )),
    ];

    let cdpCallFunctionResult: Protocol.Runtime.CallFunctionOnResponse;
    try {
      cdpCallFunctionResult = await this.cdpClient.sendCommand(
        'Runtime.callFunctionOn',
        {
          functionDeclaration: callFunctionAndSerializeScript,
          awaitPromise,
          arguments: thisAndArgumentsList,
          serializationOptions: Realm.#getSerializationOptions(
            Protocol.Runtime.SerializationOptionsSerialization.Deep,
            serializationOptions,
          ),
          executionContextId: this.executionContextId,
          userGesture: userActivation,
        },
      );
    } catch (error: any) {
      // Heuristic to determine if the problem is in the argument.
      // The check can be done on the `deserialization` step, but this approach
      // helps to save round-trips.
      if (
        error.code === CdpErrorConstants.GENERIC_ERROR &&
        [
          'Could not find object with given id',
          'Argument should belong to the same JavaScript world as target object',
          'Invalid remote object id',
        ].includes(error.message)
      ) {
        throw new NoSuchHandleException('Handle was not found.');
      }
      throw error;
    }

    if (cdpCallFunctionResult.exceptionDetails) {
      return await this.#getExceptionResult(
        cdpCallFunctionResult.exceptionDetails,
        1,
        resultOwnership,
      );
    }
    return {
      type: 'success',
      result: this.cdpToBidiValue(cdpCallFunctionResult, resultOwnership),
      realm: this.realmId,
    };
  }

  async deserializeForCdp(
    localValue: Script.LocalValue,
  ): Promise<Protocol.Runtime.CallArgument> {
    if ('handle' in localValue && localValue.handle) {
      return {objectId: localValue.handle};
      // We tried to find a handle value but failed
      // This allows us to have exhaustive switch on `localValue.type`
    } else if ('handle' in localValue || 'sharedId' in localValue) {
      throw new NoSuchHandleException('Handle was not found.');
    }

    switch (localValue.type) {
      case 'undefined':
        return {unserializableValue: 'undefined'};
      case 'null':
        return {unserializableValue: 'null'};
      case 'string':
        return {value: localValue.value};
      case 'number':
        if (localValue.value === 'NaN') {
          return {unserializableValue: 'NaN'};
        } else if (localValue.value === '-0') {
          return {unserializableValue: '-0'};
        } else if (localValue.value === 'Infinity') {
          return {unserializableValue: 'Infinity'};
        } else if (localValue.value === '-Infinity') {
          return {unserializableValue: '-Infinity'};
        }
        return {
          value: localValue.value,
        };
      case 'boolean':
        return {value: Boolean(localValue.value)};
      case 'bigint':
        return {
          unserializableValue: `BigInt(${JSON.stringify(localValue.value)})`,
        };
      case 'date':
        return {
          unserializableValue: `new Date(Date.parse(${JSON.stringify(
            localValue.value,
          )}))`,
        };
      case 'regexp':
        return {
          unserializableValue: `new RegExp(${JSON.stringify(
            localValue.value.pattern,
          )}, ${JSON.stringify(localValue.value.flags)})`,
        };
      case 'map': {
        // TODO: If none of the nested keys and values has a remote
        // reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          localValue.value,
        );
        const {result} = await this.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (...args: Protocol.Runtime.CallArgument[]) => {
                const result = new Map();

                for (let i = 0; i < args.length; i += 2) {
                  result.set(args[i], args[i + 1]);
                }

                return result;
              },
            ),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: this.executionContextId,
          },
        );
        // TODO(#375): Release `result.objectId` after using.
        return {objectId: result.objectId};
      }
      case 'object': {
        // TODO: If none of the nested keys and values has a remote
        // reference, serialize to `unserializableValue` without CDP roundtrip.
        const keyValueArray = await this.#flattenKeyValuePairs(
          localValue.value,
        );

        const {result} = await this.cdpClient.sendCommand(
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
              },
            ),
            awaitPromise: false,
            arguments: keyValueArray,
            returnByValue: false,
            executionContextId: this.executionContextId,
          },
        );
        // TODO(#375): Release `result.objectId` after using.
        return {objectId: result.objectId};
      }
      case 'array': {
        // TODO: If none of the nested items has a remote reference,
        // serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(localValue.value);

        const {result} = await this.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (...args: Protocol.Runtime.CallArgument[]) => args,
            ),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: this.executionContextId,
          },
        );
        // TODO(#375): Release `result.objectId` after using.
        return {objectId: result.objectId};
      }
      case 'set': {
        // TODO: if none of the nested items has a remote reference,
        // serialize to `unserializableValue` without CDP roundtrip.
        const args = await this.#flattenValueList(localValue.value);

        const {result} = await this.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              (...args: Protocol.Runtime.CallArgument[]) => new Set(args),
            ),
            awaitPromise: false,
            arguments: args,
            returnByValue: false,
            executionContextId: this.executionContextId,
          },
        );
        // TODO(#375): Release `result.objectId` after using.
        return {objectId: result.objectId};
      }

      case 'channel': {
        const channelProxy = new ChannelProxy(localValue.value, this.#logger);
        const channelProxySendMessageHandle = await channelProxy.init(
          this,
          this.#eventManager,
        );
        return {objectId: channelProxySendMessageHandle};
      }

      // TODO(#375): Dispose of nested objects.
    }

    // Intentionally outside to handle unknown types
    throw new Error(
      `Value ${JSON.stringify(localValue)} is not deserializable.`,
    );
  }

  async #getExceptionResult(
    exceptionDetails: Protocol.Runtime.ExceptionDetails,
    lineOffset: number,
    resultOwnership: Script.ResultOwnership,
  ): Promise<Script.EvaluateResultException> {
    return {
      exceptionDetails: await this.#serializeCdpExceptionDetails(
        exceptionDetails,
        lineOffset,
        resultOwnership,
      ),
      realm: this.realmId,
      type: 'exception',
    };
  }

  static #getSerializationOptions(
    serialization: Protocol.Runtime.SerializationOptionsSerialization,
    serializationOptions: Script.SerializationOptions,
  ): Protocol.Runtime.SerializationOptions {
    return {
      serialization,
      additionalParameters:
        Realm.#getAdditionalSerializationParameters(serializationOptions),
      ...Realm.#getMaxObjectDepth(serializationOptions),
    };
  }

  static #getAdditionalSerializationParameters(
    serializationOptions: Script.SerializationOptions,
  ) {
    const additionalParameters: {
      maxNodeDepth?: Script.SerializationOptions['maxDomDepth'];
      includeShadowTree?: Script.SerializationOptions['includeShadowTree'];
    } = {};

    if (serializationOptions.maxDomDepth !== undefined) {
      additionalParameters['maxNodeDepth'] =
        serializationOptions.maxDomDepth === null
          ? 1000
          : serializationOptions.maxDomDepth;
    }

    if (serializationOptions.includeShadowTree !== undefined) {
      additionalParameters['includeShadowTree'] =
        serializationOptions.includeShadowTree;
    }

    return additionalParameters;
  }

  static #getMaxObjectDepth(serializationOptions: Script.SerializationOptions) {
    return serializationOptions.maxObjectDepth === undefined ||
      serializationOptions.maxObjectDepth === null
      ? {}
      : {maxDepth: serializationOptions.maxObjectDepth};
  }

  async #releaseObject(handle: Script.Handle) {
    try {
      await this.cdpClient.sendCommand('Runtime.releaseObject', {
        objectId: handle,
      });
    } catch (error: any) {
      // Heuristic to determine if the problem is in the unknown handler.
      // Ignore the error if so.
      if (
        !(
          error.code === CdpErrorConstants.GENERIC_ERROR &&
          error.message === 'Invalid remote object id'
        )
      ) {
        throw error;
      }
    }
  }

  async disown(handle: Script.Handle) {
    // Disowning an object from different realm does nothing.
    if (this.realmStorage.knownHandlesToRealmMap.get(handle) !== this.realmId) {
      return;
    }

    await this.#releaseObject(handle);

    this.realmStorage.knownHandlesToRealmMap.delete(handle);
  }

  dispose(): void {
    if (!this.isHidden()) {
      this.#registerEvent({
        type: 'event',
        method: ChromiumBidi.Script.EventNames.RealmDestroyed,
        params: {
          realm: this.realmId,
        },
      });
    }
  }
}
