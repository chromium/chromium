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

import type {
  CommonDataTypes,
  Message,
  Session,
} from '../../../protocol/protocol.js';
import type {BidiServer} from '../../BidiServer.js';
import {Buffer} from '../../../utils/buffer.js';
import {IdWrapper} from '../../../utils/idWrapper.js';
import {OutgoingBidiMessage} from '../../OutgoingBidiMessage.js';

import {SubscriptionManager} from './SubscriptionManager.js';

class EventWrapper extends IdWrapper {
  readonly #contextId: CommonDataTypes.BrowsingContext | null;
  readonly #event: Promise<Message.EventMessage>;

  constructor(
    event: Promise<Message.EventMessage>,
    contextId: CommonDataTypes.BrowsingContext | null
  ) {
    super();
    this.#contextId = contextId;
    this.#event = event;
  }

  get contextId(): CommonDataTypes.BrowsingContext | null {
    return this.#contextId;
  }

  get event(): Promise<Message.EventMessage> {
    return this.#event;
  }
}

export interface IEventManager {
  registerEvent(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ): Promise<void>;

  registerPromiseEvent(
    event: Promise<Message.EventMessage>,
    contextId: CommonDataTypes.BrowsingContext | null,
    eventName: string
  ): Promise<void>;

  subscribe(
    events: Session.SubscribeParametersEvent[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void>;

  unsubscribe(
    events: Session.SubscribeParametersEvent[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void>;
}

/**
 * Maps event name to a desired buffer length.
 */
const eventBufferLength: ReadonlyMap<string, number> = new Map([
  ['log.entryAdded', 100],
]);

export class EventManager implements IEventManager {
  /**
   * Maps event name to a set of contexts where this event already happened.
   * Needed for getting buffered events from all the contexts in case of
   * subscripting to all contexts.
   */
  #eventToContextsMap: Map<
    string,
    Set<CommonDataTypes.BrowsingContext | null>
  > = new Map();
  /**
   * Maps `eventName` + `browsingContext` to buffer. Used to get buffered events
   * during subscription. Channel-agnostic.
   */
  #eventBuffers: Map<string, Buffer<EventWrapper>> = new Map();
  /**
   * Maps `eventName` + `browsingContext` + `channel` to last sent event id.
   * Used to avoid sending duplicated events when user
   * subscribes -> unsubscribes -> subscribes.
   */
  #lastMessageSent: Map<string, number> = new Map();
  #subscriptionManager: SubscriptionManager;
  #bidiServer: BidiServer;

  constructor(bidiServer: BidiServer) {
    this.#bidiServer = bidiServer;
    this.#subscriptionManager = new SubscriptionManager(
      bidiServer.getBrowsingContextStorage()
    );
  }

  /**
   * Returns consistent key to be used to access value maps.
   */
  #getMapKey(
    eventName: string,
    browsingContext: CommonDataTypes.BrowsingContext | null,
    channel: string | null | undefined = undefined
  ) {
    return JSON.stringify({eventName, browsingContext, channel});
  }

  async registerEvent(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ): Promise<void> {
    await this.registerPromiseEvent(
      Promise.resolve(event),
      contextId,
      event.method
    );
  }

  async registerPromiseEvent(
    event: Promise<Message.EventMessage>,
    contextId: CommonDataTypes.BrowsingContext | null,
    eventName: Session.SubscribeParametersEvent
  ): Promise<void> {
    const eventWrapper = new EventWrapper(event, contextId);
    const sortedChannels =
      this.#subscriptionManager.getChannelsSubscribedToEvent(
        eventName,
        contextId
      );
    this.#bufferEvent(eventWrapper, eventName);
    // Send events to channels in the subscription priority.
    for (const channel of sortedChannels) {
      this.#bidiServer.emitOutgoingMessage(
        OutgoingBidiMessage.createFromPromise(event, channel)
      );
      this.#markEventSent(eventWrapper, channel, eventName);
    }
  }

  async subscribe(
    eventNames: Session.SubscribeParametersEvent[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void> {
    // First check if all the contexts are known.
    for (const contextId of contextIds) {
      if (contextId !== null) {
        // Assert the context is known. Throw exception otherwise.
        this.#bidiServer.getBrowsingContextStorage().getKnownContext(contextId);
      }
    }

    for (const eventName of eventNames) {
      for (const contextId of contextIds) {
        this.#subscriptionManager.subscribe(eventName, contextId, channel);
        for (const eventWrapper of this.#getBufferedEvents(
          eventName,
          contextId,
          channel
        )) {
          // The order of the events is important.
          this.#bidiServer.emitOutgoingMessage(
            OutgoingBidiMessage.createFromPromise(eventWrapper.event, channel)
          );
          this.#markEventSent(eventWrapper, channel, eventName);
        }
      }
    }
  }

  async unsubscribe(
    eventNames: Session.SubscribeParametersEvent[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void> {
    this.#subscriptionManager.unsubscribeAll(eventNames, contextIds, channel);
  }

  /**
   * If the event is buffer-able, put it in the buffer.
   */
  #bufferEvent(eventWrapper: EventWrapper, eventName: string) {
    if (!eventBufferLength.has(eventName)) {
      // Do nothing if the event is no buffer-able.
      return;
    }
    const bufferMapKey = this.#getMapKey(eventName, eventWrapper.contextId);
    if (!this.#eventBuffers.has(bufferMapKey)) {
      this.#eventBuffers.set(
        bufferMapKey,
        new Buffer<EventWrapper>(eventBufferLength.get(eventName)!)
      );
    }
    this.#eventBuffers.get(bufferMapKey)!.add(eventWrapper);
    // Add the context to the list of contexts having `eventName` events.
    if (!this.#eventToContextsMap.has(eventName)) {
      this.#eventToContextsMap.set(eventName, new Set());
    }
    this.#eventToContextsMap.get(eventName)!.add(eventWrapper.contextId);
  }

  /**
   * If the event is buffer-able, mark it as sent to the given contextId and channel.
   */
  #markEventSent(
    eventWrapper: EventWrapper,
    channel: string | null,
    eventName: string
  ) {
    if (!eventBufferLength.has(eventName)) {
      // Do nothing if the event is no buffer-able.
      return;
    }

    const lastSentMapKey = this.#getMapKey(
      eventName,
      eventWrapper.contextId,
      channel
    );
    this.#lastMessageSent.set(
      lastSentMapKey,
      Math.max(this.#lastMessageSent.get(lastSentMapKey) ?? 0, eventWrapper.id)
    );
  }

  /**
   * Returns events which are buffered and not yet sent to the given channel events.
   */
  #getBufferedEvents(
    eventName: string,
    contextId: CommonDataTypes.BrowsingContext | null,
    channel: string | null
  ): EventWrapper[] {
    const bufferMapKey = this.#getMapKey(eventName, contextId);
    const lastSentMapKey = this.#getMapKey(eventName, contextId, channel);
    const lastSentMessageId =
      this.#lastMessageSent.get(lastSentMapKey) ?? -Infinity;

    const result: EventWrapper[] =
      this.#eventBuffers
        .get(bufferMapKey)
        ?.get()
        .filter((wrapper) => wrapper.id > lastSentMessageId) ?? [];

    if (contextId === null) {
      // For global subscriptions, events buffered in each context should be sent back.
      Array.from(this.#eventToContextsMap.get(eventName)?.keys() ?? [])
        .filter(
          (_contextId) =>
            // Events without context are already in the result.
            _contextId !== null &&
            // Events from deleted contexts should not be sent.
            this.#bidiServer
              .getBrowsingContextStorage()
              .hasKnownContext(_contextId)
        )
        .map((_contextId) =>
          this.#getBufferedEvents(eventName, _contextId, channel)
        )
        .forEach((events) => result.push(...events));
    }
    return result.sort((e1, e2) => e1.id - e2.id);
  }
}
