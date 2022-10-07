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
  #subscriptionManager: SubscriptionManager;
  #bidiServer: IBidiServer;

  constructor(bidiServer: IBidiServer) {
    this.#bidiServer = bidiServer;
    this.#subscriptionManager = new SubscriptionManager();
  }

  async sendEvent(
    event: Message.EventMessage,
    contextId: CommonDataTypes.BrowsingContext | null
  ): Promise<void> {
    const sortedChannels =
      this.#subscriptionManager.getChannelsSubscribedToEvent(
        event.method,
        contextId
      );

    // Send events to channels in the subscription priority.
    for (const channel of sortedChannels) {
      await this.#bidiServer.sendMessage(event, channel);
    }
  }

  async subscribe(
    events: string[],
    contextIds: (CommonDataTypes.BrowsingContext | null)[],
    channel: string | null
  ): Promise<void> {
    for (let event of events) {
      for (let contextId of contextIds) {
        this.#subscriptionManager.subscribe(event, contextId, channel);
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
}
