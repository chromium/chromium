/**
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
 */
import type Protocol from 'devtools-protocol';

import {uuidv4} from '../../../utils/uuid.js';
import {Network, NoSuchInterceptException} from '../../../protocol/protocol.js';
import {DefaultMap} from '../../../utils/DefaultMap.js';
import type {EventManager} from '../events/EventManager.js';

import {NetworkRequest} from './NetworkRequest.js';

/** Stores network and intercept maps. */
export class NetworkStorage {
  /**
   * Map of request ID to NetworkRequest objects. Needed as long as information
   * about requests comes from different events.
   */
  readonly #requestMap: DefaultMap<Network.Request, NetworkRequest>;

  /** A map to define the properties of active network intercepts. */
  readonly #interceptMap: Map<
    Network.Intercept,
    {
      urlPatterns: Network.UrlPattern[];
      phases: Network.InterceptPhase[];
    }
  >;

  /** A map to track the requests which are actively being blocked. */
  readonly #blockedRequestMap: Map<
    Network.Request,
    {
      request: Network.Request;
      phase: Network.InterceptPhase;
      response: Network.ResponseData;
    }
  >;

  constructor(eventManager: EventManager) {
    this.#requestMap = new DefaultMap(
      (requestId) => new NetworkRequest(requestId, eventManager)
    );
    this.#interceptMap = new Map();
    this.#blockedRequestMap = new Map();
  }

  // XXX: Replace getters with custom operations, follow suit of Browsing Context Storage.
  get requestMap() {
    return this.#requestMap;
  }

  disposeRequestMap() {
    for (const request of this.#requestMap.values()) {
      request.dispose();
    }

    this.#requestMap.clear();
  }

  /**
   * Adds the given entry to the intercept map.
   * URL patterns are assumed to be parsed.
   *
   * @return The intercept ID.
   */
  addIntercept(value: {
    urlPatterns: Network.UrlPattern[];
    phases: Network.InterceptPhase[];
  }): Network.Intercept {
    const intercept: Network.Intercept = uuidv4();

    this.#interceptMap.set(intercept, value);

    return intercept;
  }

  /**
   * Removes the given intercept from the intercept map.
   * Throws NoSuchInterceptException if the intercept does not exist.
   */
  removeIntercept(intercept: Network.Intercept) {
    if (!this.#interceptMap.has(intercept)) {
      throw new NoSuchInterceptException(
        `Intercept '${intercept}' does not exist.`
      );
    }

    this.#interceptMap.delete(intercept);
  }

  /** Gets parameters for CDP 'Fetch.enable' command from the intercept map. */
  getFetchEnableParams(): Protocol.Fetch.EnableRequest {
    const patterns: Protocol.Fetch.RequestPattern[] = [];

    for (const value of this.#interceptMap.values()) {
      for (const urlPatternSpec of value.urlPatterns) {
        const urlPattern: string =
          NetworkStorage.cdpFromSpecUrlPattern(urlPatternSpec);
        for (const phase of value.phases) {
          if (phase === Network.InterceptPhase.AuthRequired) {
            patterns.push({
              urlPattern,
            });
          } else {
            patterns.push({
              urlPattern,
              requestStage: NetworkStorage.requestStageFromPhase(phase),
            });
          }
        }
      }
    }

    return {
      patterns,
      // If there's at least one intercept that requires auth, enable the
      // 'Fetch.authRequired' event.
      handleAuthRequests: [...this.#interceptMap.values()].some((param) => {
        return param.phases.includes(Network.InterceptPhase.AuthRequired);
      }),
    };
  }

  /** Converts a URL pattern from the spec to a CDP URL pattern. */
  static cdpFromSpecUrlPattern(urlPattern: Network.UrlPattern): string {
    switch (urlPattern.type) {
      case 'string':
        return urlPattern.pattern;
      case 'pattern':
        return NetworkStorage.buildUrlPatternString(urlPattern);
    }
  }

  static buildUrlPatternString({
    protocol,
    hostname,
    port,
    pathname,
    search,
  }: Network.UrlPatternPattern): string {
    let url: string = '';

    if (protocol) {
      url += `${protocol}`;

      if (!protocol.endsWith(':')) {
        url += ':';
      }

      url += '//';
    }

    if (hostname) {
      url += hostname;
    }

    if (port) {
      url += `:${port}`;
    }

    if (pathname) {
      if (!pathname.startsWith('/')) {
        url += '/';
      }

      url += pathname;
    }

    if (search) {
      if (!search.startsWith('?')) {
        url += '?';
      }

      url += `${search}`;
    }

    return url;
  }

  /**
   * Maps spec Network.InterceptPhase to CDP Fetch.RequestStage.
   * AuthRequired has no CDP equivalent..
   */
  static requestStageFromPhase(
    phase: Network.InterceptPhase
  ): Protocol.Fetch.RequestStage {
    switch (phase) {
      case Network.InterceptPhase.BeforeRequestSent:
        return 'Request';
      case Network.InterceptPhase.ResponseStarted:
        return 'Response';
      case Network.InterceptPhase.AuthRequired:
        throw new Error(
          'AuthRequired is not a valid intercept phase for request stage.'
        );
    }
  }

  // XXX: Replace getters with custom operations, follow suit of Browsing Context Storage.
  get blockedRequestMap() {
    return this.#blockedRequestMap;
  }
}
