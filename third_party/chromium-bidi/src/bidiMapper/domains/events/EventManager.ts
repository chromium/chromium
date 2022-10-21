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

import { CommonDataTypes, Message } from '../protocol/bidiProtocolTypes';
import { IBidiServer } from '../../utils/bidiServer';
import { SubscriptionManager } from './SubscriptionManager';
import { IdWrapper } from '../../../utils/idWrapper';
import { Buffer } from '../../../utils/buffer';
import { BrowsingContextStorage } from '../context/browsingContextStorage';

class EventWrapper extends IdWrapper {
  readonly #contextId: CommonDataTypes.BrowsingContext | null;
  readonly #event: Message.EventMessage;

  constructor(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ) {
    super();
    this.#contextId = contextId;
    this.#event = event;
  }

  get contextId(): CommonDataTypes.BrowsingContext | null {
    return this.#contextId;
  }

  get event(): Message.EventMessage {
    return this.#event;
  }
}

export interface IEventManager {
  sendEvent(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ): Promise<void>;

  subscribe(
    events: string[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void>;

  unsubscribe(
    event: string[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void>;
}

export class EventManager implements IEventManager {
  /**
   * Maps event name to a desired buffer length.
   */
  static readonly #eventBufferLength: Map<string, number> = new Map([
    ['log.entryAdded', 100],
  ]);
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
  #bidiServer: IBidiServer;

  constructor(bidiServer: IBidiServer) {
    this.#bidiServer = bidiServer;
    this.#subscriptionManager = new SubscriptionManager();
  }

  /**
   * Returns consistent key to be used to access value maps.
   */
  static #getMapKey(
    eventName: string,
    browsingContext: CommonDataTypes.BrowsingContext | null,
    channel: string | null | undefined = undefined
  ) {
    return JSON.stringify({ eventName, browsingContext, channel });
  }

  async sendEvent(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ): Promise<void> {
    const eventWrapper = new EventWrapper(event, contextId);
    const sortedChannels =
      this.#subscriptionManager.getChannelsSubscribedToEvent(
        event.method,
        contextId
      );
    this.#bufferEvent(eventWrapper);
    // Send events to channels in the subscription priority.
    for (const channel of sortedChannels) {
      await this.#bidiServer.sendMessage(event, channel);
      this.#markEventSent(eventWrapper, channel);
    }
  }

  async subscribe(
    eventNames: string[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void> {
    for (let eventName of eventNames) {
      for (let contextId of contextIds) {
        if (
          contextId !== null &&
          !BrowsingContextStorage.hasKnownContext(contextId)
        ) {
          // Unknown context. Do nothing.
          continue;
        }
        this.#subscriptionManager.subscribe(eventName, contextId, channel);
        for (let eventWrapper of this.#getBufferedEvents(
          eventName,
          contextId,
          channel
        )) {
          // The order of the events is important.
          await this.#bidiServer.sendMessage(eventWrapper.event, channel);
          this.#markEventSent(eventWrapper, channel);
        }
      }
    }
  }

  async unsubscribe(
    events: string[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void> {
    for (let event of events) {
      for (let contextId of contextIds) {
        this.#subscriptionManager.unsubscribe(event, contextId, channel);
      }
    }
  }

  /**
   * If the event is buffer-able, put it in the buffer.
   */
  #bufferEvent(eventWrapper: EventWrapper) {
    const eventName = eventWrapper.event.method;
    if (!EventManager.#eventBufferLength.has(eventName)) {
      // Do nothing if the event is no buffer-able.
      return;
    }
    const bufferMapKey = EventManager.#getMapKey(
      eventName,
      eventWrapper.contextId
    );
    if (!this.#eventBuffers.has(bufferMapKey)) {
      this.#eventBuffers.set(
        bufferMapKey,
        new Buffer<EventWrapper>(
          EventManager.#eventBufferLength.get(eventName)!
        )
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
  #markEventSent(eventWrapper: EventWrapper, channel: string | null) {
    const eventName = eventWrapper.event.method;
    if (!EventManager.#eventBufferLength.has(eventName)) {
      // Do nothing if the event is no buffer-able.
      return;
    }

    const lastSentMapKey = EventManager.#getMapKey(
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
    const bufferMapKey = EventManager.#getMapKey(eventName, contextId);
    const lastSentMapKey = EventManager.#getMapKey(
      eventName,
      contextId,
      channel
    );
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
        // Events without context are already in the result.
        .filter((_contextId) => _contextId !== null)
        .map((_contextId) =>
          this.#getBufferedEvents(eventName, _contextId, channel)
        )
        .forEach((events) => result.push(...events));
    }
    return result.sort((e1, e2) => e1.id - e2.id);
  }
}
