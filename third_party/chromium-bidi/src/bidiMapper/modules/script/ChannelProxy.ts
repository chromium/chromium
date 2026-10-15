/*
 * Copyright 2023 Google LLC.
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
 *
 */

import {Protocol} from 'devtools-protocol';

import {ChromiumBidi, Script} from '../../../protocol/protocol.js';
import {LogType, type LoggerFn} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {EventManager} from '../session/EventManager.js';

import type {Realm} from './Realm.js';

/**
 * Used to send messages from realm to BiDi user.
 */
export class ChannelProxy {
  readonly #properties: Script.ChannelProperties;

  readonly #id = uuidv4();
  readonly #logger?: LoggerFn;

  constructor(channel: Script.ChannelProperties, logger?: LoggerFn) {
    this.#properties = channel;
    this.#logger = logger;
  }

  /**
   * Creates a channel proxy in the given realm, initialises listener and
   * returns a handle to `sendMessage` delegate.
   */
  async init(realm: Realm, eventManager: EventManager): Promise<Script.Handle> {
    const channelHandle = await ChannelProxy.#createAndGetHandleInRealm(realm);
    const sendMessageHandle = await ChannelProxy.#createSendMessageHandle(
      realm,
      channelHandle,
    );

    void this.#startListener(realm, channelHandle, eventManager);
    return sendMessageHandle;
  }

  /** Gets a ChannelProxy from window and returns its handle. */
  async startListenerFromWindow(
    realm: Realm,
    eventManager: EventManager,
  ): Promise<void> {
    try {
      const channelHandle = await this.#getHandleFromWindow(realm);
      void this.#startListener(realm, channelHandle, eventManager);
    } catch (error) {
      this.#logger?.(LogType.debugError)?.(error);
    }
  }

  /**
   * Evaluation string which creates a ChannelProxy object on the client side.
   */
  static #createChannelProxyEvalStr() {
    const functionStr = String(() => {
      const queue: unknown[] = [];
      // Track pending `getMessage` resolvers in FIFO order so multiple in-flight
      // `Runtime.callFunctionOn` requests can be fulfilled before navigation.
      const resolvers: ((message: unknown) => void)[] = [];

      return {
        /**
         * Gets a promise, which is resolved as soon as a message occurs
         * in the queue.
         */
        async getMessage(): Promise<unknown> {
          // Return the oldest buffered message immediately when non-empty.
          if (queue.length > 0) {
            return queue.shift();
          }
          // Otherwise suspend until `sendMessage` pushes the next message.
          return await new Promise<unknown>((resolve) => {
            resolvers.push(resolve);
          });
        },

        /**
         * Adds a message to the queue.
         * Resolves the pending promise if needed.
         */
        sendMessage(message: unknown) {
          // Fulfill the oldest waiting `getMessage` call directly if one exists.
          const resolve = resolvers.shift();
          if (resolve !== undefined) {
            resolve(message);
            return;
          }
          // Otherwise buffer the message for a future `getMessage` call.
          queue.push(message);
        },
      };
    });

    return `(${functionStr})()`;
  }

  /** Creates a ChannelProxy in the given realm. */
  static async #createAndGetHandleInRealm(
    realm: Realm,
  ): Promise<Script.Handle> {
    const createChannelHandleResult = await realm.cdpClient.sendCommand(
      'Runtime.evaluate',
      {
        expression: this.#createChannelProxyEvalStr(),
        contextId: realm.executionContextId,
        serializationOptions: {
          serialization:
            Protocol.Runtime.SerializationOptionsSerialization.IdOnly,
        },
      },
    );
    if (
      createChannelHandleResult.exceptionDetails ||
      createChannelHandleResult.result.objectId === undefined
    ) {
      throw new Error(`Cannot create channel`);
    }
    return createChannelHandleResult.result.objectId;
  }

  /** Gets a handle to `sendMessage` delegate from the ChannelProxy handle. */
  static async #createSendMessageHandle(
    realm: Realm,
    channelHandle: Script.Handle,
  ): Promise<Script.Handle> {
    const sendMessageArgResult = await realm.cdpClient.sendCommand(
      'Runtime.callFunctionOn',
      {
        functionDeclaration: String(
          (channelHandle: {sendMessage: (message: string) => void}) => {
            return channelHandle.sendMessage;
          },
        ),
        arguments: [{objectId: channelHandle}],
        executionContextId: realm.executionContextId,
        serializationOptions: {
          serialization:
            Protocol.Runtime.SerializationOptionsSerialization.IdOnly,
        },
      },
    );
    // TODO: check for exceptionDetails.
    return sendMessageArgResult.result.objectId!;
  }

  /** Starts listening for the channel events of the provided ChannelProxy. */
  async #startListener(
    realm: Realm,
    channelHandle: Script.Handle,
    eventManager: EventManager,
  ) {
    // Wrap each CDP response or error so pre-fetched commands never cause
    // unhandled promise rejections when the execution context is destroyed.
    interface CommandResult {
      message?: Protocol.Runtime.CallFunctionOnResponse;
      error?: unknown;
    }
    // Keep a pipeline of pending `getMessage` CDP calls in the realm so back-to-back
    // channel messages sent right before navigation are returned without waiting
    // for a mapper-to-renderer round-trip.
    const pendingCommands: Promise<CommandResult>[] = [];
    const maxPending = 10;

    // Issue a single `getMessage` CDP call and capture its outcome.
    const createCommand = async (): Promise<CommandResult> => {
      try {
        const message = await realm.cdpClient.sendCommand(
          'Runtime.callFunctionOn',
          {
            functionDeclaration: String(
              async (channelHandle: {getMessage: () => Promise<unknown>}) =>
                await channelHandle.getMessage(),
            ),
            arguments: [
              {
                objectId: channelHandle,
              },
            ],
            awaitPromise: true,
            executionContextId: realm.executionContextId,
            serializationOptions: {
              serialization:
                Protocol.Runtime.SerializationOptionsSerialization.Deep,
              maxDepth:
                this.#properties.serializationOptions?.maxObjectDepth ??
                undefined,
            },
          },
        );
        // Return the resolved CDP response for ordered processing.
        return {message};
      } catch (error) {
        // Capture CDP transport/context errors to stop the listener loop cleanly.
        return {error};
      }
    };

    // Pre-populate the pipeline of waiting `getMessage` calls in the realm.
    for (let i = 0; i < maxPending; i++) {
      pendingCommands.push(createCommand());
    }

    // noinspection InfiniteLoopJS
    for (;;) {
      // Await the oldest in-flight `getMessage` call to preserve FIFO order.
      const result = await pendingCommands.shift()!;
      if (result.error) {
        // If an error is thrown, then the channel is permanently broken, so we
        // exit the loop.
        this.#logger?.(LogType.debugError)?.(result.error);
        break;
      }
      // Replenish the pipeline with a new waiting `getMessage` call.
      pendingCommands.push(createCommand());

      const message = result.message!;
      if (message.exceptionDetails) {
        // Stop listening if `getMessage` threw an exception in the realm.
        this.#logger?.(LogType.debugError)?.(
          new Error('Runtime.callFunctionOn in ChannelProxy', {
            cause: message.exceptionDetails,
          }),
        );
        break;
      }

      for (const browsingContext of realm.associatedBrowsingContexts) {
        eventManager.registerEvent(
          {
            type: 'event',
            method: ChromiumBidi.Script.EventNames.Message,
            params: {
              channel: this.#properties.channel,
              data: realm.cdpToBidiValue(
                message,
                this.#properties.ownership ?? Script.ResultOwnership.None,
              ),
              source: realm.source,
            },
          },
          browsingContext.id,
        );
      }
    }
  }

  /**
   * Returns a handle of ChannelProxy from window's property which was set there
   * by `getEvalInWindowStr`. If window property is not set yet, sets a promise
   * resolver to the window property, so that `getEvalInWindowStr` can resolve
   * the promise later on with the channel.
   * This is needed because `getEvalInWindowStr` can be called before or
   * after this method.
   */
  async #getHandleFromWindow(realm: Realm) {
    const channelHandleResult = await realm.cdpClient.sendCommand(
      'Runtime.callFunctionOn',
      {
        functionDeclaration: String((id: string) => {
          const w = window as unknown as {
            [key: string]: unknown;
          };
          if (w[id] === undefined) {
            // The channelProxy is not created yet. Create a promise, put the
            // resolver to window property and return the promise.
            // `getEvalInWindowStr` will resolve the promise later.
            return new Promise((resolve) => (w[id] = resolve));
          }
          // The channelProxy is already created by `getEvalInWindowStr` and
          // is set into window property. Return it.
          const channelProxy = w[id];
          delete w[id];
          return channelProxy;
        }),
        arguments: [{value: this.#id}],
        executionContextId: realm.executionContextId,
        awaitPromise: true,
        serializationOptions: {
          serialization:
            Protocol.Runtime.SerializationOptionsSerialization.IdOnly,
        },
      },
    );
    if (
      channelHandleResult.exceptionDetails !== undefined ||
      channelHandleResult.result.objectId === undefined
    ) {
      throw new Error(`ChannelHandle not found in window["${this.#id}"]`);
    }
    return channelHandleResult.result.objectId;
  }

  /**
   * String to be evaluated to create a ProxyChannel and put it to window.
   * Returns the delegate `sendMessage`. Used to provide an argument for preload
   * script. Does the following:
   * 1. Creates a ChannelProxy.
   * 2. Puts the ChannelProxy to window['${this.#id}'] or resolves the promise
   *    by calling delegate stored in window['${this.#id}'].
   *    This is needed because `#getHandleFromWindow` can be called before or
   *    after this method.
   * 3. Returns the delegate `sendMessage` of the created ChannelProxy.
   */
  getEvalInWindowStr() {
    const delegate = String(
      (id: string, channelProxy: {sendMessage: unknown}) => {
        const w = window as unknown as {
          [key: string]: unknown;
        };
        if (w[id] === undefined) {
          // `#getHandleFromWindow` is not initialized yet, and will get the
          // channelProxy later.
          w[id] = channelProxy;
        } else {
          // `#getHandleFromWindow` is already set a delegate to window property
          // and is waiting for it to be called with the channelProxy.
          (w[id] as (c: unknown) => void)(channelProxy);
          delete w[id];
        }
        return channelProxy.sendMessage;
      },
    );
    const channelProxyEval = ChannelProxy.#createChannelProxyEvalStr();
    return `(${delegate})('${this.#id}',${channelProxyEval})`;
  }
}
