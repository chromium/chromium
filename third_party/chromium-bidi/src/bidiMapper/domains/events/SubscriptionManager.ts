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
  BrowsingContext,
  CDP,
  CommonDataTypes,
  Log,
  Message,
  Session,
} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from '../context/browsingContextStorage.js';
import InvalidArgumentException = Message.InvalidArgumentException;

/**
 * Returns the cartesian product of the given arrays.
 *
 * Example:
 *   cartesian([1, 2], ['a', 'b']); => [[1, 'a'], [1, 'b'], [2, 'a'], [2, 'b']]
 */
function cartesianProduct(...a: any[][]) {
  return a.reduce((a: unknown[], b: unknown[]) =>
    a.flatMap((d: unknown) => b.map((e: unknown) => [d, e].flat()))
  );
}

/** Expands "AllEvents" events into atomic events. */
function unrollEvents(
  events: Session.SubscribeParametersEvent[]
): Session.SubscribeParametersEvent[] {
  const allEvents: Session.SubscribeParametersEvent[] = [];

  for (const event of events) {
    switch (event) {
      case BrowsingContext.AllEvents:
        allEvents.push(...Object.values(BrowsingContext.EventNames));
        break;
      case CDP.AllEvents:
        allEvents.push(...Object.values(CDP.EventNames));
        break;
      case Log.AllEvents:
        allEvents.push(...Object.values(Log.EventNames));
        break;
      default:
        allEvents.push(event);
    }
  }

  return allEvents;
}

export class SubscriptionManager {
  #subscriptionPriority = 0;
  // BrowsingContext `null` means the event has subscription across all the
  // browsing contexts.
  // Channel `null` means no `channel` should be added.
  #channelToContextToEventMap: Map<
    string | null,
    Map<
      CommonDataTypes.BrowsingContext | null,
      Map<Session.SubscribeParametersEvent, number>
    >
  > = new Map();
  #browsingContextStorage: BrowsingContextStorage;

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  getChannelsSubscribedToEvent(
    eventMethod: Session.SubscribeParametersEvent,
    contextId: CommonDataTypes.BrowsingContext | null
  ): (string | null)[] {
    const prioritiesAndChannels = Array.from(
      this.#channelToContextToEventMap.keys()
    )
      .map((channel) => ({
        priority: this.#getEventSubscriptionPriorityForChannel(
          eventMethod,
          contextId,
          channel
        ),
        channel,
      }))
      .filter(({priority}) => priority !== null) as {
      priority: number;
      channel: string | null;
    }[];

    // Sort channels by priority.
    return prioritiesAndChannels
      .sort((a, b) => a.priority - b.priority)
      .map(({channel}) => channel);
  }

  #getEventSubscriptionPriorityForChannel(
    eventMethod: Session.SubscribeParametersEvent,
    contextId: CommonDataTypes.BrowsingContext | null,
    channel: string | null
  ): null | number {
    const contextToEventMap = this.#channelToContextToEventMap.get(channel);
    if (contextToEventMap === undefined) {
      return null;
    }

    const maybeTopLevelContextId = this.#findTopLevelContextId(contextId);

    // `null` covers global subscription.
    const relevantContexts = [...new Set([null, maybeTopLevelContextId])];

    // Get all the subscription priorities.
    const priorities: number[] = relevantContexts
      .map((c) => contextToEventMap.get(c)?.get(eventMethod))
      .filter((p) => p !== undefined) as number[];

    if (priorities.length === 0) {
      // Not subscribed, return null.
      return null;
    }

    // Return minimal priority.
    return Math.min(...priorities);
  }

  #findTopLevelContextId(
    contextId: CommonDataTypes.BrowsingContext | null
  ): CommonDataTypes.BrowsingContext | null {
    if (contextId === null) {
      return null;
    }
    const maybeContext = this.#browsingContextStorage.findContext(contextId);
    const parentId = maybeContext?.parentId ?? null;
    if (parentId !== null) {
      return this.#findTopLevelContextId(parentId);
    }
    return contextId;
  }

  subscribe(
    event: Session.SubscribeParametersEvent,
    contextId: CommonDataTypes.BrowsingContext | null,
    channel: string | null
  ): void {
    // All the subscriptions are handled on the top-level contexts.
    contextId = this.#findTopLevelContextId(contextId);

    if (event === BrowsingContext.AllEvents) {
      Object.values(BrowsingContext.EventNames).map((specificEvent) =>
        this.subscribe(specificEvent, contextId, channel)
      );
      return;
    }
    if (event === CDP.AllEvents) {
      Object.values(CDP.EventNames).map((specificEvent) =>
        this.subscribe(specificEvent, contextId, channel)
      );
      return;
    }
    if (event === Log.AllEvents) {
      Object.values(Log.EventNames).map((specificEvent) =>
        this.subscribe(specificEvent, contextId, channel)
      );
      return;
    }

    if (!this.#channelToContextToEventMap.has(channel)) {
      this.#channelToContextToEventMap.set(channel, new Map());
    }
    const contextToEventMap = this.#channelToContextToEventMap.get(channel)!;

    if (!contextToEventMap.has(contextId)) {
      contextToEventMap.set(contextId, new Map());
    }
    const eventMap = contextToEventMap.get(contextId)!;

    // Do not re-subscribe to events to keep the priority.
    if (eventMap.has(event)) {
      return;
    }

    eventMap.set(event, this.#subscriptionPriority++);
  }

  /**
   * Unsubscribes atomically from all events in the given contexts and channel.
   */
  unsubscribeAll(
    events: Session.SubscribeParametersEvent[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ) {
    // Assert all contexts are known.
    for (const contextId of contextIds) {
      if (contextId !== null) {
        this.#browsingContextStorage.getKnownContext(contextId);
      }
    }

    const eventContextPairs: Array<
      [
        eventName: Session.SubscribeParametersEvent,
        contextId: CommonDataTypes.BrowsingContext | null
      ]
    > = cartesianProduct(unrollEvents(events), contextIds);

    // Assert all unsubscriptions are valid.
    // If any of the unsubscriptions are invalid, do not unsubscribe from anything.
    eventContextPairs
      .map(([event, contextId]) =>
        this.#checkUnsubscribe(event, contextId, channel)
      )
      .forEach((unsubscribe) => unsubscribe());
  }

  /**
   * Unsubscribes from the event in the given context and channel.
   * Syntactic sugar for "unsubscribeAll".
   */
  unsubscribe(
    eventName: Session.SubscribeParametersEvent,
    contextId: CommonDataTypes.BrowsingContext | null,
    channel: string | null
  ) {
    this.unsubscribeAll([eventName], [contextId], channel);
  }

  #checkUnsubscribe(
    event: Session.SubscribeParametersEvent,
    contextId: CommonDataTypes.BrowsingContext | null,
    channel: string | null
  ): Function {
    // All the subscriptions are handled on the top-level contexts.
    contextId = this.#findTopLevelContextId(contextId);

    if (!this.#channelToContextToEventMap.has(channel)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${contextId}. No subscription found.`
      );
    }
    const contextToEventMap = this.#channelToContextToEventMap.get(channel)!;

    if (!contextToEventMap.has(contextId)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${contextId}. No subscription found.`
      );
    }
    const eventMap = contextToEventMap.get(contextId)!;

    if (!eventMap.has(event)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${contextId}. No subscription found.`
      );
    }

    return () => {
      eventMap.delete(event);

      // Clean up maps if empty.
      if (eventMap.size === 0) {
        contextToEventMap.delete(event);
      }
      if (contextToEventMap.size === 0) {
        this.#channelToContextToEventMap.delete(channel);
      }
    };
  }
}
