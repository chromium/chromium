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

import type {BidiPlusChannel} from '../../../protocol/chromium-bidi.js';
import {
  ChromiumBidi,
  InvalidArgumentException,
  type BrowsingContext,
} from '../../../protocol/protocol.js';
import type {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {isCdpEvent} from './events.js';

/**
 * Returns the cartesian product of the given arrays.
 *
 * Example:
 *   cartesian([1, 2], ['a', 'b']); => [[1, 'a'], [1, 'b'], [2, 'a'], [2, 'b']]
 */
export function cartesianProduct(...a: any[][]) {
  return a.reduce((a: unknown[], b: unknown[]) =>
    a.flatMap((d) => b.map((e) => [d, e].flat()))
  );
}

/** Expands "AllEvents" events into atomic events. */
export function unrollEvents(
  events: ChromiumBidi.EventNames[]
): ChromiumBidi.EventNames[] {
  const allEvents = new Set<ChromiumBidi.EventNames>();

  function addEvents(events: ChromiumBidi.EventNames[]) {
    for (const event of events) {
      allEvents.add(event);
    }
  }

  for (const event of events) {
    switch (event) {
      case ChromiumBidi.BiDiModule.BrowsingContext:
        addEvents(Object.values(ChromiumBidi.BrowsingContext.EventNames));
        break;
      case ChromiumBidi.BiDiModule.Log:
        addEvents(Object.values(ChromiumBidi.Log.EventNames));
        break;
      case ChromiumBidi.BiDiModule.Network:
        addEvents(Object.values(ChromiumBidi.Network.EventNames));
        break;
      case ChromiumBidi.BiDiModule.Script:
        addEvents(Object.values(ChromiumBidi.Script.EventNames));
        break;
      default:
        allEvents.add(event);
    }
  }

  return [...allEvents.values()];
}

export class SubscriptionManager {
  #subscriptionPriority = 0;
  // BrowsingContext `null` means the event has subscription across all the
  // browsing contexts.
  // Channel `null` means no `channel` should be added.
  #channelToContextToEventMap = new Map<
    BidiPlusChannel,
    Map<
      BrowsingContext.BrowsingContext | null,
      Map<ChromiumBidi.EventNames, number>
    >
  >();
  #browsingContextStorage: BrowsingContextStorage;

  constructor(browsingContextStorage: BrowsingContextStorage) {
    this.#browsingContextStorage = browsingContextStorage;
  }

  getChannelsSubscribedToEvent(
    eventMethod: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null
  ): BidiPlusChannel[] {
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
      channel: BidiPlusChannel;
    }[];

    // Sort channels by priority.
    return prioritiesAndChannels
      .sort((a, b) => a.priority - b.priority)
      .map(({channel}) => channel);
  }

  #getEventSubscriptionPriorityForChannel(
    eventMethod: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null,
    channel: BidiPlusChannel
  ): null | number {
    const contextToEventMap = this.#channelToContextToEventMap.get(channel);
    if (contextToEventMap === undefined) {
      return null;
    }

    const maybeTopLevelContextId =
      this.#browsingContextStorage.findTopLevelContextId(contextId);

    // `null` covers global subscription.
    const relevantContexts = [...new Set([null, maybeTopLevelContextId])];

    // Get all the subscription priorities.
    const priorities: number[] = relevantContexts
      .map((context) => {
        // Get the priority for exact event name
        const priority = contextToEventMap.get(context)?.get(eventMethod);
        // For CDP we can't provide specific event name when subscribing
        // to the module directly.
        // Because of that we need to see event `cdp` exits in the map.
        if (isCdpEvent(eventMethod)) {
          const cdpPriority = contextToEventMap
            .get(context)
            ?.get(ChromiumBidi.BiDiModule.Cdp);
          // If we subscribe to the event directly and `cdp` module as well
          // priority will be different we take minimal priority
          return priority && cdpPriority
            ? Math.min(priority, cdpPriority)
            : // At this point we know that we have subscribed
              // to only one of the two
              priority ?? cdpPriority;
        }
        return priority;
      })
      .filter((p) => p !== undefined) as number[];

    if (priorities.length === 0) {
      // Not subscribed, return null.
      return null;
    }

    // Return minimal priority.
    return Math.min(...priorities);
  }

  /**
   * @param module BiDi+ module
   * @param contextId `null` == globally subscribed
   *
   * @returns
   */
  isSubscribedTo(
    moduleOrEvent: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null = null
  ): boolean {
    const topLevelContext =
      this.#browsingContextStorage.findTopLevelContextId(contextId);

    for (const browserContextToEventMap of this.#channelToContextToEventMap.values()) {
      for (const [id, eventMap] of browserContextToEventMap.entries()) {
        // Not subscribed to this context or globally
        if (topLevelContext !== id && id !== null) {
          continue;
        }

        for (const event of eventMap.keys()) {
          // This also covers the `cdp` case where
          // we don't unroll the event names
          if (
            // Event explicitly subscribed
            event === moduleOrEvent ||
            // Event subscribed via module
            event === moduleOrEvent.split('.').at(0) ||
            // Event explicitly subscribed compared to module
            event.split('.').at(0) === moduleOrEvent
          ) {
            return true;
          }
        }
      }
    }

    return false;
  }

  subscribe(
    event: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null,
    channel: BidiPlusChannel
  ): void {
    // All the subscriptions are handled on the top-level contexts.
    contextId = this.#browsingContextStorage.findTopLevelContextId(contextId);

    // Check if subscribed event is a whole module
    switch (event) {
      case ChromiumBidi.BiDiModule.BrowsingContext:
        Object.values(ChromiumBidi.BrowsingContext.EventNames).map(
          (specificEvent) => this.subscribe(specificEvent, contextId, channel)
        );
        return;
      case ChromiumBidi.BiDiModule.Log:
        Object.values(ChromiumBidi.Log.EventNames).map((specificEvent) =>
          this.subscribe(specificEvent, contextId, channel)
        );
        return;
      case ChromiumBidi.BiDiModule.Network:
        Object.values(ChromiumBidi.Network.EventNames).map((specificEvent) =>
          this.subscribe(specificEvent, contextId, channel)
        );
        return;
      case ChromiumBidi.BiDiModule.Script:
        Object.values(ChromiumBidi.Script.EventNames).map((specificEvent) =>
          this.subscribe(specificEvent, contextId, channel)
        );
        return;
      default:
      // Intentionally left empty.
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
    events: ChromiumBidi.EventNames[],
    contextIds: (BrowsingContext.BrowsingContext | null)[],
    channel: BidiPlusChannel
  ) {
    // Assert all contexts are known.
    for (const contextId of contextIds) {
      if (contextId !== null) {
        this.#browsingContextStorage.getContext(contextId);
      }
    }

    const eventContextPairs: [
      eventName: ChromiumBidi.EventNames,
      contextId: BrowsingContext.BrowsingContext | null,
    ][] = cartesianProduct(unrollEvents(events), contextIds);

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
    eventName: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null,
    channel: BidiPlusChannel
  ) {
    this.unsubscribeAll([eventName], [contextId], channel);
  }

  #checkUnsubscribe(
    event: ChromiumBidi.EventNames,
    contextId: BrowsingContext.BrowsingContext | null,
    channel: BidiPlusChannel
  ): () => void {
    // All the subscriptions are handled on the top-level contexts.
    contextId = this.#browsingContextStorage.findTopLevelContextId(contextId);

    if (!this.#channelToContextToEventMap.has(channel)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${
          contextId === null ? 'null' : contextId
        }. No subscription found.`
      );
    }
    const contextToEventMap = this.#channelToContextToEventMap.get(channel)!;

    if (!contextToEventMap.has(contextId)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${
          contextId === null ? 'null' : contextId
        }. No subscription found.`
      );
    }
    const eventMap = contextToEventMap.get(contextId)!;

    if (!eventMap.has(event)) {
      throw new InvalidArgumentException(
        `Cannot unsubscribe from ${event}, ${
          contextId === null ? 'null' : contextId
        }. No subscription found.`
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
