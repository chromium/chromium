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
import type {Protocol} from 'devtools-protocol';

import {Network, NoSuchInterceptException} from '../../../protocol/protocol.js';
import {URLPattern} from '../../../utils/UrlPattern.js';
import {uuidv4} from '../../../utils/uuid.js';

import type {NetworkRequest} from './NetworkRequest.js';

/** Stores network and intercept maps. */
export class NetworkStorage {
  /**
   * A map from network request ID to Network Request objects.
   * Needed as long as information about requests comes from different events.
   */
  readonly #requestMap = new Map<Network.Request, NetworkRequest>();

  /** A map from intercept ID to track active network intercepts. */
  readonly #interceptMap = new Map<
    Network.Intercept,
    {
      urlPatterns: Network.UrlPattern[];
      phases: Network.AddInterceptParameters['phases'];
    }
  >();

  /** A map from network request ID to track actively blocked requests. */
  readonly #blockedRequestMap = new Map<
    Network.Request,
    {
      // intercept request id; form: 'interception-job-1.0'
      request: Protocol.Fetch.RequestId;
      phase: Network.InterceptPhase;
      response: Network.ResponseData;
    }
  >();

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
    phases: Network.AddInterceptParameters['phases'];
  }): Network.Intercept {
    // Check if the given intercept entry already exists.
    for (const [
      interceptId,
      {urlPatterns, phases},
    ] of this.#interceptMap.entries()) {
      if (
        JSON.stringify(value.urlPatterns) === JSON.stringify(urlPatterns) &&
        JSON.stringify(value.phases) === JSON.stringify(phases)
      ) {
        return interceptId;
      }
    }

    const interceptId: Network.Intercept = uuidv4();
    this.#interceptMap.set(interceptId, value);

    return interceptId;
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

  /** Returns true if there's at least one added intercept. */
  hasIntercepts() {
    return this.#interceptMap.size > 0;
  }

  /** Gets parameters for CDP 'Fetch.enable' command from the intercept map. */
  getFetchEnableParams(): Protocol.Fetch.EnableRequest {
    const patterns: Protocol.Fetch.RequestPattern[] = [];

    for (const value of this.#interceptMap.values()) {
      for (const phase of value.phases) {
        const requestStage = NetworkStorage.requestStageFromPhase(phase);

        if (value.urlPatterns.length === 0) {
          patterns.push({
            urlPattern: '*',
            requestStage,
          });
          continue;
        }
        for (const urlPatternSpec of value.urlPatterns) {
          const urlPattern =
            NetworkStorage.cdpFromSpecUrlPattern(urlPatternSpec);

          patterns.push({
            urlPattern,
            requestStage,
          });
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

  getRequest(id: Network.Request): NetworkRequest | undefined {
    return this.#requestMap.get(id);
  }

  addRequest(request: NetworkRequest) {
    this.#requestMap.set(request.requestId, request);
  }

  deleteRequest(id: Network.Request) {
    const request = this.#requestMap.get(id);
    if (request) {
      request.dispose();
      this.#requestMap.delete(id);
    }
  }

  /** Returns true if there's at least one network request. */
  hasNetworkRequests() {
    return this.#requestMap.size > 0;
  }

  /** Returns true if there's at least one blocked network request. */
  hasBlockedRequests() {
    return this.#blockedRequestMap.size > 0;
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
    if (!protocol && !hostname && !port && !pathname && !search) {
      return '*';
    }

    let url: string = '';

    if (protocol) {
      url += protocol;

      if (!protocol.endsWith(':')) {
        url += ':';
      }

      if (NetworkStorage.isSpecialScheme(protocol)) {
        url += '//';
      }
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

      url += search;
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
      case Network.InterceptPhase.AuthRequired:
        return 'Response';
    }
  }

  /**
   * Returns true if the given protocol is special.
   * Special protocols are those that have a default port.
   *
   * Example inputs: 'http', 'http:'
   *
   * @see https://url.spec.whatwg.org/#special-scheme
   */
  static isSpecialScheme(protocol: string): boolean {
    return ['ftp', 'file', 'http', 'https', 'ws', 'wss'].includes(
      protocol.replace(/:$/, '')
    );
  }

  addBlockedRequest(
    requestId: Network.Request,
    value: {
      request: Protocol.Fetch.RequestId;
      phase: Network.InterceptPhase;
      response: Network.ResponseData;
    }
  ) {
    this.#blockedRequestMap.set(requestId, value);
  }

  removeBlockedRequest(requestId: Network.Request) {
    this.#blockedRequestMap.delete(requestId);
  }

  /**
   * Returns the blocked request associated with the given network ID, if any.
   */
  getBlockedRequest(networkId: Network.Request):
    | {
        request: Protocol.Fetch.RequestId;
        phase: Network.InterceptPhase;
        response: Network.ResponseData;
      }
    | undefined {
    return this.#blockedRequestMap.get(networkId);
  }

  /** #@see https://w3c.github.io/webdriver-bidi/#get-the-network-intercepts */
  getNetworkIntercepts(
    requestId: Network.Request,
    phase?: Network.InterceptPhase
  ): Network.Intercept[] {
    const request = this.#requestMap.get(requestId);
    if (!request) {
      return [];
    }

    const interceptIds: Network.Intercept[] = [];

    for (const [
      interceptId,
      {phases, urlPatterns},
    ] of this.#interceptMap.entries()) {
      if (phase && phases.includes(phase)) {
        if (urlPatterns.length === 0) {
          interceptIds.push(interceptId);
        } else if (
          urlPatterns.some((urlPattern) =>
            NetworkStorage.matchUrlPattern(urlPattern, request.url)
          )
        ) {
          interceptIds.push(interceptId);
        }
      }
    }

    return interceptIds;
  }

  /** Matches the given URLPattern against the given URL. */
  static matchUrlPattern(
    urlPattern: Network.UrlPattern,
    url: string | undefined
  ): boolean {
    switch (urlPattern.type) {
      case 'string':
        return urlPattern.pattern === url;
      case 'pattern': {
        return (
          new URLPattern({
            protocol: urlPattern.protocol,
            hostname: urlPattern.hostname,
            port: urlPattern.port,
            pathname: urlPattern.pathname,
            search: urlPattern.search,
          }).exec(url) !== null
        );
      }
    }
  }
}
