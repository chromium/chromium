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

import {
  ChromiumBidi,
  type BrowsingContext,
  InvalidArgumentException,
} from '../../../protocol/protocol.js';
import {DefaultMap} from '../../../utils/DefaultMap.js';
import {Buffer} from '../../../utils/Buffer.js';
import {IdWrapper} from '../../../utils/IdWrapper.js';
import type {Result} from '../../../utils/result.js';
import type {BidiServer} from '../../BidiServer.js';
import {OutgoingMessage} from '../../OutgoingMessage.js';

import {SubscriptionManager} from './SubscriptionManager.js';

class EventWrapper {
  readonly #idWrapper = new IdWrapper();
  readonly #contextId: BrowsingContext.BrowsingContext | null;
  readonly #event: Promise<Result<ChromiumBidi.Event>>;

  constructor(
    event: Promise<Result<ChromiumBidi.Event>>,
    contextId: BrowsingContext.BrowsingContext | null
  ) {
    this.#event = event;
    this.#contextId = contextId;
  }

  get id(): number {
    return this.#idWrapper.id;
  }

  get contextId(): BrowsingContext.BrowsingContext | null {
    return this.#contextId;
  }

  get event(): Promise<Result<ChromiumBidi.Event>> {
    return this.#event;
  }
}

/**
 * Maps event name to a desired buffer length.
 */
const eventBufferLength: ReadonlyMap<ChromiumBidi.EventNames, number> = new Map(
  [[ChromiumBidi.Log.EventNames.LogEntryAdded, 100]]
);

export class EventManager {
  /**
   * Maps event name to a set of contexts where this event already happened.
   * Needed for getting buffered events from all the contexts in case of
   * subscripting to all contexts.
   */
  #eventToContextsMap = new DefaultMap<
    string,
    Set<BrowsingContext.BrowsingContext | null>
  >(() => new Set());
  /**
   * Maps `eventName` + `browsingContext` to buffer. Used to get buffered events
   * during subscription. Channel-agnostic.
   */
  #eventBuffers = new Map<string, Buffer<EventWrapper>>();
  /**
   * Maps `eventName` + `browsingContext` + `channel` to last sent event id.
   * Used to avoid sending duplicated events when user
   * subscribes -> unsubscribes -> subscribes.
   */
  #lastMessageSent = new Map<string, number>();
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
  static #getMapKey(
    eventName: ChromiumBidi.EventNames,
    browsingContext: BrowsingContext.BrowsingContext | null,
    channel?: string | null
  ) {
    return JSON.stringify({eventName, browsingContext, channel});
  }

  registerEvent(
    event: ChromiumBidi.Event,
    contextId: BrowsingContext.BrowsingContext | null
  ): void {
    this.registerPromiseEvent(
      Promise.resolve({
        kind: 'success',
        value: event,
      }),
      contextId,
      event.method as ChromiumBidi.EventNames
    );
  }

  registerPromiseEvent(
    event: Promise<Result<ChromiumBidi.Event>>,
    contextId: BrowsingContext.BrowsingContext | null,
    eventName: ChromiumBidi.EventNames
  ): void {
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
        OutgoingMessage.createFromPromise(event, channel),
        eventName
      );
      this.#markEventSent(eventWrapper, channel, eventName);
    }
  }

  subscribe(
    eventNames: ChromiumBidi.EventNames[],
    contextIds: (BrowsingContext.BrowsingContext | null)[],
    channel: string | null
  ): void {
    for (const name of eventNames) {
      checkEventName(name);
    }

    // First check if all the contexts are known.
    for (const contextId of contextIds) {
      if (contextId !== null) {
        // Assert the context is known. Throw exception otherwise.
        this.#bidiServer.getBrowsingContextStorage().getContext(contextId);
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
            OutgoingMessage.createFromPromise(eventWrapper.event, channel),
            eventName
          );
          this.#markEventSent(eventWrapper, channel, eventName);
        }
      }
    }
  }

  unsubscribe(
    eventNames: ChromiumBidi.EventNames[],
    contextIds: (BrowsingContext.BrowsingContext | null)[],
    channel: string | null
  ) {
    for (const name of eventNames) {
      checkEventName(name);
    }
    this.#subscriptionManager.unsubscribeAll(eventNames, contextIds, channel);
  }

  /**
   * If the event is buffer-able, put it in the buffer.
   */
  #bufferEvent(eventWrapper: EventWrapper, eventName: ChromiumBidi.EventNames) {
    if (!eventBufferLength.has(eventName)) {
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
        new Buffer<EventWrapper>(eventBufferLength.get(eventName)!)
      );
    }
    this.#eventBuffers.get(bufferMapKey)!.add(eventWrapper);
    // Add the context to the list of contexts having `eventName` events.
    this.#eventToContextsMap.get(eventName).add(eventWrapper.contextId);
  }

  /**
   * If the event is buffer-able, mark it as sent to the given contextId and channel.
   */
  #markEventSent(
    eventWrapper: EventWrapper,
    channel: string | null,
    eventName: ChromiumBidi.EventNames
  ) {
    if (!eventBufferLength.has(eventName)) {
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
    eventName: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null,
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
      Array.from(this.#eventToContextsMap.get(eventName).keys())
        .filter(
          (_contextId) =>
            // Events without context are already in the result.
            _contextId !== null &&
            // Events from deleted contexts should not be sent.
            this.#bidiServer.getBrowsingContextStorage().hasContext(_contextId)
        )
        .map((_contextId) =>
          this.#getBufferedEvents(eventName, _contextId, channel)
        )
        .forEach((events) => result.push(...events));
    }
    return result.sort((e1, e2) => e1.id - e2.id);
  }
}

const EVENT_NAMES = new Set([
  // keep-sorted start
  ...Object.values(ChromiumBidi.BiDiModule),
  ...Object.values(ChromiumBidi.BrowsingContext.EventNames),
  ...Object.values(ChromiumBidi.Log.EventNames),
  ...Object.values(ChromiumBidi.Network.EventNames),
  ...Object.values(ChromiumBidi.Script.EventNames),
  // keep-sorted end
]);

function checkEventName(name: string) {
  if (
    !EVENT_NAMES.has(name as never) &&
    !name.startsWith('cdp.') &&
    name !== 'cdp'
  ) {
    throw new InvalidArgumentException(`Unknown event: ${name}`);
  }
}
