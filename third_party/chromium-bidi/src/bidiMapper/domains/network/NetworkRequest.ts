/*
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
 *
 */

/**
 * @fileoverview `NetworkRequest` represents a single network request and keeps
 * track of all the related CDP events.
 */
import type Protocol from 'devtools-protocol';

import {Deferred} from '../../../utils/Deferred.js';
import type {EventManager} from '../events/EventManager.js';
import {
  type Network,
  type BrowsingContext,
  ChromiumBidi,
} from '../../../protocol/protocol.js';
import type {Result} from '../../../utils/result.js';
import {assert} from '../../../utils/assert.js';

/** Abstracts one individual network request. */
export class NetworkRequest {
  static #unknown = 'UNKNOWN';

  /**
   * Each network request has an associated request id, which is a string
   * uniquely identifying that request.
   *
   * The identifier for a request resulting from a redirect matches that of the
   * request that initiated it.
   */
  readonly requestId: Network.Request;

  #servedFromCache = false;
  #redirectCount: number;

  #eventManager: EventManager;

  #request: {
    info?: Protocol.Network.RequestWillBeSentEvent;
    extraInfo?: Protocol.Network.RequestWillBeSentExtraInfoEvent;
  } = {};

  #response: {
    info?: Protocol.Network.ResponseReceivedEvent;
    extraInfo?: Protocol.Network.ResponseReceivedExtraInfoEvent;
  } = {};

  #beforeRequestSentDeferred = new Deferred<Result<void>>();
  #responseCompletedDeferred = new Deferred<Result<void>>();

  constructor(
    requestId: Network.Request,
    eventManager: EventManager,
    redirectCount = 0
  ) {
    this.requestId = requestId;
    this.#eventManager = eventManager;
    this.#redirectCount = redirectCount;
  }

  get url(): string | undefined {
    return this.#response.info?.response.url ?? this.#request.info?.request.url;
  }

  get redirectCount() {
    return this.#redirectCount;
  }

  isRedirecting(): boolean {
    return Boolean(this.#request.info);
  }

  handleRedirect(): void {
    this.#emitEventsIfReady(true);
    this.#responseCompletedDeferred.resolve({
      kind: 'error',
      error: new Error('Redirects produce no response'),
    });
  }

  #emitEventsIfReady(wasRedirected = false) {
    const requestExtraInfoCompleted =
      // Flush redirects
      wasRedirected ||
      Boolean(this.#request.extraInfo) ||
      // Requests from cache don't have extra info
      this.#servedFromCache ||
      // Sometimes there is no extra info and the response
      // is the only place we can find out
      Boolean(this.#response.info && !this.#response.info.hasExtraInfo);

    if (this.#request.info && requestExtraInfoCompleted) {
      this.#beforeRequestSentDeferred.resolve({
        kind: 'success',
        value: undefined,
      });
    }

    const responseExtraInfoCompleted =
      Boolean(this.#response.extraInfo) ||
      // Response from cache don't have extra info
      this.#servedFromCache ||
      // Don't expect extra info if the flag is false
      Boolean(this.#response.info && !this.#response.info.hasExtraInfo);

    if (this.#response.info && responseExtraInfoCompleted) {
      this.#responseCompletedDeferred.resolve({
        kind: 'success',
        value: undefined,
      });
    }
  }

  onRequestWillBeSentEvent(event: Protocol.Network.RequestWillBeSentEvent) {
    this.#request.info = event;
    this.#queueBeforeRequestSentEvent();
    this.#emitEventsIfReady();
  }

  onRequestWillBeSentExtraInfoEvent(
    event: Protocol.Network.RequestWillBeSentExtraInfoEvent
  ) {
    this.#request.extraInfo = event;
    this.#emitEventsIfReady();
  }

  onResponseReceivedExtraInfoEvent(
    event: Protocol.Network.ResponseReceivedExtraInfoEvent
  ) {
    this.#response.extraInfo = event;
    this.#emitEventsIfReady();
  }

  onResponseReceivedEvent(event: Protocol.Network.ResponseReceivedEvent) {
    this.#response.info = event;
    this.#queueResponseCompletedEvent();
    this.#emitEventsIfReady();
  }

  onServedFromCache() {
    this.#servedFromCache = true;
    this.#emitEventsIfReady();
  }

  onLoadingFailedEvent(event: Protocol.Network.LoadingFailedEvent) {
    this.#beforeRequestSentDeferred.resolve({
      kind: 'success',
      value: undefined,
    });
    this.#responseCompletedDeferred.resolve({
      kind: 'error',
      error: new Error('Network event loading failed'),
    });

    this.#eventManager.registerEvent(
      {
        type: 'event',
        method: ChromiumBidi.Network.EventNames.FetchError,
        params: {
          ...this.#getBaseEventParams(),
          errorText: event.errorText,
        },
      },
      this.#context
    );
  }

  dispose() {
    const result = {
      kind: 'error' as const,
      error: new Error('Network processor detached'),
    };
    this.#beforeRequestSentDeferred.resolve(result);
    this.#responseCompletedDeferred.resolve(result);
  }

  get #context() {
    return this.#request.info?.frameId ?? null;
  }

  #getBaseEventParams(): Network.BaseParameters {
    return {
      // TODO: implement.
      isBlocked: false,
      context: this.#context,
      navigation: this.#getNavigationId(),
      // TODO: implement.
      redirectCount: this.#redirectCount,
      request: this.#getRequestData(),
      // Timestamp should be in milliseconds, while CDP provides it in seconds.
      timestamp: Math.round((this.#request.info?.wallTime ?? 0) * 1000),
    };
  }

  #getNavigationId(): BrowsingContext.Navigation | null {
    if (
      !this.#request.info ||
      !this.#request.info.loaderId ||
      // When we navigate all CDP network events have `loaderId`
      // CDP's `loaderId` and `requestId` match when
      // that request triggered the loading
      this.#request.info.loaderId !== this.#request.info.requestId
    ) {
      return null;
    }
    return this.#request.info.loaderId;
  }

  #getRequestData(): Network.RequestData {
    const cookies = this.#request.extraInfo
      ? NetworkRequest.#getCookies(this.#request.extraInfo.associatedCookies)
      : [];

    return {
      request: this.#request.info?.requestId ?? NetworkRequest.#unknown,
      url: this.#request.info?.request.url ?? NetworkRequest.#unknown,
      method: this.#request.info?.request.method ?? NetworkRequest.#unknown,
      headers: NetworkRequest.#getHeaders(this.#request.info?.request.headers),
      cookies,
      // TODO: implement.
      headersSize: -1,
      // TODO: implement.
      bodySize: 0,
      timings: this.#getTimings(),
    };
  }
  // TODO: implement.
  #getTimings(): Network.FetchTimingInfo {
    return {
      timeOrigin: 0,
      requestTime: 0,
      redirectStart: 0,
      redirectEnd: 0,
      fetchStart: 0,
      dnsStart: 0,
      dnsEnd: 0,
      connectStart: 0,
      connectEnd: 0,
      tlsStart: 0,
      requestStart: 0,
      responseStart: 0,
      responseEnd: 0,
    };
  }

  #queueBeforeRequestSentEvent() {
    if (this.#isIgnoredEvent()) {
      return;
    }
    this.#eventManager.registerPromiseEvent(
      this.#beforeRequestSentDeferred.then((result) => {
        if (result.kind === 'success') {
          return {
            kind: 'success',
            value: Object.assign(this.#getBeforeRequestEvent(), {
              type: 'event' as const,
            }),
          };
        }
        return result;
      }),
      this.#context,
      ChromiumBidi.Network.EventNames.BeforeRequestSent
    );
  }

  #getBeforeRequestEvent(): Network.BeforeRequestSent {
    assert(this.#request.info, 'RequestWillBeSentEvent is not set');

    return {
      method: ChromiumBidi.Network.EventNames.BeforeRequestSent,
      params: {
        ...this.#getBaseEventParams(),
        initiator: {
          type: NetworkRequest.#getInitiatorType(
            this.#request.info.initiator.type
          ),
        },
      },
    };
  }

  #queueResponseCompletedEvent() {
    if (this.#isIgnoredEvent()) {
      return;
    }
    this.#eventManager.registerPromiseEvent(
      this.#responseCompletedDeferred.then((result) => {
        if (result.kind === 'success') {
          return {
            kind: 'success',
            value: Object.assign(this.#getResponseReceivedEvent(), {
              type: 'event' as const,
            }),
          };
        }
        return result;
      }),
      this.#context,
      ChromiumBidi.Network.EventNames.ResponseCompleted
    );
  }

  #getResponseReceivedEvent(): Network.ResponseCompleted {
    assert(this.#request.info, 'RequestWillBeSentEvent is not set');
    assert(this.#response.info, 'ResponseReceivedEvent is not set');

    // Chromium sends wrong extraInfo events for responses served from cache.
    // See https://github.com/puppeteer/puppeteer/issues/9965 and
    // https://crbug.com/1340398.
    if (this.#response.info.response.fromDiskCache) {
      this.#response.extraInfo = undefined;
    }

    const headers = NetworkRequest.#getHeaders(
      this.#response.info.response.headers
    );

    return {
      method: ChromiumBidi.Network.EventNames.ResponseCompleted,
      params: {
        ...this.#getBaseEventParams(),
        response: {
          url: this.#response.info.response.url ?? NetworkRequest.#unknown,
          protocol: this.#response.info.response.protocol ?? '',
          status:
            this.#response.extraInfo?.statusCode ??
            this.#response.info.response.status,
          statusText: this.#response.info.response.statusText,
          fromCache:
            this.#response.info.response.fromDiskCache ||
            this.#response.info.response.fromPrefetchCache ||
            this.#servedFromCache,
          headers,
          mimeType: this.#response.info.response.mimeType,
          bytesReceived: this.#response.info.response.encodedDataLength,
          headersSize: this.#computeResponseHeadersSize(headers),
          // TODO: consider removing from spec.
          bodySize: 0,
          content: {
            // TODO: consider removing from spec.
            size: 0,
          },
        },
      },
    };
  }

  #computeResponseHeadersSize(headers: Network.Header[]): number {
    return headers.reduce((total, header) => {
      return (
        total + header.name.length + header.value.value.length + 4 // 4 = ': ' + '\r\n'
      );
    }, 0);
  }

  #isIgnoredEvent(): boolean {
    return this.#request.info?.request.url.endsWith('/favicon.ico') ?? false;
  }

  static #getHeaders(headers?: Protocol.Network.Headers): Network.Header[] {
    if (!headers) {
      return [];
    }

    return Object.entries(headers).map(([name, value]) => ({
      name,
      value: {
        type: 'string',
        value,
      },
    }));
  }

  static #getInitiatorType(
    initiatorType: Protocol.Network.Initiator['type']
  ): Network.Initiator['type'] {
    switch (initiatorType) {
      case 'parser':
      case 'script':
      case 'preflight':
        return initiatorType;
      default:
        return 'other';
    }
  }

  static #getCookies(
    associatedCookies: Protocol.Network.BlockedCookieWithReason[]
  ): Network.Cookie[] {
    return associatedCookies
      .filter(({blockedReasons}) => {
        return !Array.isArray(blockedReasons) || blockedReasons.length === 0;
      })
      .map(({cookie}) => {
        return {
          name: cookie.name,
          value: {
            type: 'string',
            value: cookie.value,
          },
          domain: cookie.domain,
          path: cookie.path,
          expires: cookie.expires,
          size: cookie.size,
          httpOnly: cookie.httpOnly,
          secure: cookie.secure,
          sameSite: NetworkRequest.#getCookiesSameSite(cookie.sameSite),
        };
      });
  }

  static #getCookiesSameSite(
    cdpSameSiteValue?: string
  ): Network.Cookie['sameSite'] {
    switch (cdpSameSiteValue) {
      case 'Strict':
        return 'strict';
      case 'Lax':
        return 'lax';
      default:
        return 'none';
    }
  }
}
