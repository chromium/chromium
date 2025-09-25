/*
 * Copyright 2025 Google LLC.
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

import {NoSuchNetworkCollectorException} from '../../../protocol/ErrorResponse.js';
import type {
  Browser,
  BrowsingContext,
  Network,
} from '../../../protocol/generated/webdriver-bidi.js';
import {type LoggerFn, LogType} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';

import type {NetworkRequest} from './NetworkRequest.js';

type NetworkCollector = Network.AddDataCollectorParameters;

export class CollectorsStorage {
  readonly #collectors = new Map<string, NetworkCollector>();
  readonly #requestCollectors = new Map<Network.Request, Set<string>>();
  readonly #logger?: LoggerFn;

  constructor(logger?: LoggerFn) {
    this.#logger = logger;
  }

  addDataCollector(params: Network.AddDataCollectorParameters) {
    const collectorId = uuidv4();
    this.#collectors.set(collectorId, params);
    return collectorId;
  }

  isCollected(requestId: Network.Request, collectorId?: string) {
    if (collectorId !== undefined && !this.#collectors.has(collectorId)) {
      throw new NoSuchNetworkCollectorException(
        `Unknown collector ${collectorId}`,
      );
    }

    const requestCollectors = this.#requestCollectors.get(requestId);
    if (requestCollectors === undefined || requestCollectors.size === 0) {
      return false;
    }

    if (collectorId === undefined) {
      // There is at least 1 collector for the request.
      return true;
    }

    if (!this.#requestCollectors.get(requestId)?.has(collectorId)) {
      return false;
    }

    return true;
  }

  disown(requestId: Network.Request, collectorId?: string) {
    if (collectorId !== undefined) {
      this.#requestCollectors.get(requestId)?.delete(collectorId);
    }
    if (
      collectorId === undefined ||
      this.#requestCollectors.get(requestId)?.size === 0
    ) {
      this.#requestCollectors.delete(requestId);
    }
  }

  #getCollectorIdsForRequest(
    request: NetworkRequest,
    topLevelBrowsingContext: BrowsingContext.BrowsingContext,
    userContext: Browser.UserContext,
  ): string[] {
    const collectors = new Set<string>();
    for (const collectorId of this.#collectors.keys()) {
      const collector = this.#collectors.get(collectorId)!;

      if (!collector.userContexts && !collector.contexts) {
        // A global collector.
        collectors.add(collectorId);
      }
      if (collector.contexts?.includes(topLevelBrowsingContext)) {
        collectors.add(collectorId);
      }
      if (collector.userContexts?.includes(userContext)) {
        collectors.add(collectorId);
      }
    }

    this.#logger?.(
      LogType.debug,
      `Request ${request.id} collected by ${[...collectors.values()]}`,
    );
    return [...collectors.values()];
  }

  collectIfNeeded(
    request: NetworkRequest,
    topLevelBrowsingContext: BrowsingContext.BrowsingContext,
    userContext: Browser.UserContext,
  ) {
    const collectorIds = this.#getCollectorIdsForRequest(
      request,
      topLevelBrowsingContext,
      userContext,
    );
    if (collectorIds.length > 0) {
      this.#requestCollectors.set(request.id, new Set(collectorIds));
    }
  }

  removeDataCollector(collectorId: Network.Collector): Network.Request[] {
    if (!this.#collectors.has(collectorId)) {
      throw new NoSuchNetworkCollectorException(
        `Collector ${collectorId} does not exist`,
      );
    }
    this.#collectors.delete(collectorId);

    const releasedRequests = [];
    // Clean up collected responses.
    for (const [requestId, collectorIds] of this.#requestCollectors) {
      if (collectorIds.has(collectorId)) {
        collectorIds.delete(collectorId);
        if (collectorIds.size === 0) {
          this.#requestCollectors.delete(requestId);
          releasedRequests.push(requestId);
        }
      }
    }
    return releasedRequests;
  }
}
