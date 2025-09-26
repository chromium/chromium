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

import {
  InvalidArgumentException,
  NoSuchNetworkCollectorException,
} from '../../../protocol/ErrorResponse.js';
import type {
  Browser,
  BrowsingContext,
  Network,
} from '../../../protocol/generated/webdriver-bidi.js';
import {type LoggerFn, LogType} from '../../../utils/log.js';
import {uuidv4} from '../../../utils/uuid.js';

import type {NetworkRequest} from './NetworkRequest.js';

type NetworkCollector = Network.AddDataCollectorParameters;

// The default total data size limit in CDP.
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/inspector/inspector_network_agent.cc;drc=da1f749634c9a401cc756f36c2e6ce233e1c9b4d;l=133
const MAX_TOTAL_COLLECTED_SIZE = 200 * 1000 * 1000;

export class CollectorsStorage {
  readonly #collectors = new Map<string, NetworkCollector>();
  readonly #requestCollectors = new Map<Network.Request, Set<string>>();
  readonly #logger?: LoggerFn;

  constructor(logger?: LoggerFn) {
    this.#logger = logger;
  }

  addDataCollector(params: Network.AddDataCollectorParameters) {
    if (
      params.maxEncodedDataSize < 1 ||
      params.maxEncodedDataSize > MAX_TOTAL_COLLECTED_SIZE
    ) {
      // 200 MB is the default limit in CDP:
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/inspector/inspector_network_agent.cc;drc=da1f749634c9a401cc756f36c2e6ce233e1c9b4d;l=133
      throw new InvalidArgumentException(
        `Max encoded data size should be between 1 and ${MAX_TOTAL_COLLECTED_SIZE}`,
      );
    }
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

  #shouldCollectRequest(
    collectorId: string,
    request: NetworkRequest,
    topLevelBrowsingContext: BrowsingContext.BrowsingContext,
    userContext: Browser.UserContext,
  ): boolean {
    if (!this.#collectors.has(collectorId)) {
      throw new NoSuchNetworkCollectorException(
        `Unknown collector ${collectorId}`,
      );
    }

    const collector = this.#collectors.get(collectorId)!;
    if (
      collector.userContexts &&
      !collector.userContexts.includes(userContext)
    ) {
      // Collector is aimed for a different user context.
      return false;
    }
    if (
      collector.contexts &&
      !collector.contexts.includes(topLevelBrowsingContext)
    ) {
      // Collector is aimed for a different top-level browsing context.
      return false;
    }
    if (collector.maxEncodedDataSize < request.bytesReceived) {
      this.#logger?.(
        LogType.debug,
        `Request ${request.id} is too big for the collector ${collectorId}`,
      );
      return false;
    }

    this.#logger?.(
      LogType.debug,
      `Collector ${collectorId} collected request ${request.id}`,
    );
    return true;
  }

  collectIfNeeded(
    request: NetworkRequest,
    topLevelBrowsingContext: BrowsingContext.BrowsingContext,
    userContext: Browser.UserContext,
  ) {
    const collectorIds = [...this.#collectors.keys()].filter((collectorId) =>
      this.#shouldCollectRequest(
        collectorId,
        request,
        topLevelBrowsingContext,
        userContext,
      ),
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
