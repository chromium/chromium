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

import {Deferred} from '../../../utils/deferred.js';
import type {IEventManager} from '../events/EventManager.js';
import {
  type Network,
  type BrowsingContext,
  ChromiumBidi,
} from '../../../protocol/protocol.js';

export class NetworkRequest {
  static #unknown = 'UNKNOWN';

  /**
   * Each network request has an associated request id, which is a string
   * uniquely identifying that request.
   *
   * The identifier for a request resulting from a redirect matches that of the
   * request that initiated it.
   */
  requestId: Network.Request;

  #servedFromCache = false;
  #redirectCount = 0;

  #eventManager: IEventManager;

  #requestWillBeSentEvent?: Protocol.Network.RequestWillBeSentEvent;
  #requestWillBeSentExtraInfoEvent?: Protocol.Network.RequestWillBeSentExtraInfoEvent;
  #responseReceivedEvent?: Protocol.Network.ResponseReceivedEvent;
  #responseReceivedExtraInfoEvent?: Protocol.Network.ResponseReceivedExtraInfoEvent;

  #beforeRequestSentDeferred = new Deferred<void>();
  #responseReceivedDeferred = new Deferred<void>();

  constructor(requestId: Network.Request, eventManager: IEventManager) {
    this.requestId = requestId;
    this.#eventManager = eventManager;
  }

  onRequestWillBeSentEvent(event: Protocol.Network.RequestWillBeSentEvent) {
    if (this.#requestWillBeSentEvent !== undefined) {
      // TODO: Handle redirect event, requestId is same for the redirect chain
      return;
    }
    this.#requestWillBeSentEvent = event;

    if (this.#requestWillBeSentExtraInfoEvent !== undefined) {
      this.#beforeRequestSentDeferred.resolve();
    }

    this.#sendBeforeRequestEvent();
  }

  onRequestWillBeSentExtraInfoEvent(
    event: Protocol.Network.RequestWillBeSentExtraInfoEvent
  ) {
    if (this.#requestWillBeSentExtraInfoEvent !== undefined) {
      // TODO: Handle redirect event, requestId is same for the redirect chain
      return;
    }
    this.#requestWillBeSentExtraInfoEvent = event;

    if (this.#requestWillBeSentEvent !== undefined) {
      this.#beforeRequestSentDeferred.resolve();
    }
  }

  onResponseReceivedEventExtraInfo(
    event: Protocol.Network.ResponseReceivedExtraInfoEvent
  ) {
    if (this.#responseReceivedExtraInfoEvent !== undefined) {
      // TODO: Handle redirect event, requestId is same for the redirect chain
      return;
    }
    this.#responseReceivedExtraInfoEvent = event;

    if (this.#responseReceivedEvent !== undefined) {
      this.#responseReceivedDeferred.resolve();
    }
  }

  onResponseReceivedEvent(
    responseReceivedEvent: Protocol.Network.ResponseReceivedEvent
  ) {
    if (this.#responseReceivedEvent !== undefined) {
      // TODO: Handle redirect event, requestId is same for the redirect chain
      return;
    }
    this.#responseReceivedEvent = responseReceivedEvent;

    if (
      !responseReceivedEvent.hasExtraInfo &&
      !this.#beforeRequestSentDeferred.isFinished
    ) {
      this.#beforeRequestSentDeferred.resolve();
    }

    if (
      !responseReceivedEvent.hasExtraInfo ||
      this.#responseReceivedExtraInfoEvent !== undefined ||
      this.#servedFromCache
    ) {
      this.#responseReceivedDeferred.resolve();
    }

    this.#sendResponseReceivedEvent();
  }

  onServedFromCache() {
    if (this.#requestWillBeSentEvent !== undefined) {
      this.#beforeRequestSentDeferred.resolve();
    }

    if (this.#responseReceivedEvent !== undefined) {
      this.#responseReceivedDeferred.resolve();
    }

    this.#servedFromCache = true;
  }

  onLoadingFailedEvent(event: Protocol.Network.LoadingFailedEvent) {
    this.#beforeRequestSentDeferred.resolve();
    this.#responseReceivedDeferred.reject(event);

    this.#eventManager.registerEvent(
      {
        method: ChromiumBidi.Network.EventNames.FetchErrorEvent,
        params: {
          ...this.#getBaseEventParams(),
          errorText: event.errorText,
        },
      },
      this.#requestWillBeSentEvent?.frameId ?? null
    );
  }

  #getBaseEventParams(): Network.BaseParameters {
    return {
      // TODO: Implement.
      isBlocked: false,
      context: this.#requestWillBeSentEvent?.frameId ?? null,
      navigation: this.#getNavigationId(),
      // TODO: implement.
      redirectCount: this.#redirectCount,
      request: this.#getRequestData(),
      // Timestamp should be in milliseconds, while CDP provides it in seconds.
      timestamp: Math.round(
        (this.#requestWillBeSentEvent?.wallTime ?? 0) * 1000
      ),
    };
  }

  #getNavigationId(): BrowsingContext.Navigation | null {
    if (
      !this.#requestWillBeSentEvent ||
      !this.#requestWillBeSentEvent.loaderId ||
      // When we navigate all CDP network events have `loaderId`
      // CDP's `loaderId` and `requestId` match when
      // that request triggered the loading
      this.#requestWillBeSentEvent.loaderId !==
        this.#requestWillBeSentEvent.requestId
    ) {
      return null;
    }
    return this.#requestWillBeSentEvent.loaderId;
  }

  #getRequestData(): Network.RequestData {
    const cookies = this.#requestWillBeSentExtraInfoEvent
      ? NetworkRequest.#getCookies(
          this.#requestWillBeSentExtraInfoEvent.associatedCookies
        )
      : [];

    return {
      request:
        this.#requestWillBeSentEvent?.requestId ?? NetworkRequest.#unknown,
      url: this.#requestWillBeSentEvent?.request.url ?? NetworkRequest.#unknown,
      method:
        this.#requestWillBeSentEvent?.request.method ?? NetworkRequest.#unknown,
      headers: NetworkRequest.#getHeaders(
        this.#requestWillBeSentEvent?.request.headers
      ),
      cookies,
      // TODO: implement.
      headersSize: -1,
      // TODO: implement.
      bodySize: 0,
      timings: {
        // TODO: implement.
        timeOrigin: 0,
        // TODO: implement.
        requestTime: 0,
        // TODO: implement.
        redirectStart: 0,
        // TODO: implement.
        redirectEnd: 0,
        // TODO: implement.
        fetchStart: 0,
        // TODO: implement.
        dnsStart: 0,
        // TODO: implement.
        dnsEnd: 0,
        // TODO: implement.
        connectStart: 0,
        // TODO: implement.
        connectEnd: 0,
        // TODO: implement.
        tlsStart: 0,
        // TODO: implement.
        requestStart: 0,
        // TODO: implement.
        responseStart: 0,
        // TODO: implement.
        responseEnd: 0,
      },
    };
  }

  #sendBeforeRequestEvent() {
    if (!this.#isIgnoredEvent()) {
      this.#eventManager.registerPromiseEvent(
        this.#beforeRequestSentDeferred.then(() =>
          this.#getBeforeRequestEvent()
        ),
        this.#requestWillBeSentEvent?.frameId ?? null,
        ChromiumBidi.Network.EventNames.BeforeRequestSentEvent
      );
    }
  }

  #getBeforeRequestEvent(): Network.BeforeRequestSent {
    if (this.#requestWillBeSentEvent === undefined) {
      throw new Error('RequestWillBeSentEvent is not set');
    }

    return {
      method: ChromiumBidi.Network.EventNames.BeforeRequestSentEvent,
      params: {
        ...this.#getBaseEventParams(),
        initiator: {
          type: NetworkRequest.#getInitiatorType(
            this.#requestWillBeSentEvent.initiator.type
          ),
        },
      },
    };
  }

  #sendResponseReceivedEvent() {
    if (!this.#isIgnoredEvent()) {
      this.#eventManager.registerPromiseEvent(
        this.#responseReceivedDeferred.then(() =>
          this.#getResponseReceivedEvent()
        ),
        this.#responseReceivedEvent?.frameId ?? null,
        ChromiumBidi.Network.EventNames.ResponseCompletedEvent
      );
    }
  }

  #getResponseReceivedEvent(): Network.ResponseCompleted {
    if (this.#requestWillBeSentEvent === undefined) {
      throw new Error('RequestWillBeSentEvent is not set');
    }
    if (this.#responseReceivedEvent === undefined) {
      throw new Error('ResponseReceivedEvent is not set');
    }

    // Chromium sends wrong extraInfo events for responses served from cache.
    // See https://github.com/puppeteer/puppeteer/issues/9965 and
    // https://crbug.com/1340398.
    if (this.#responseReceivedEvent.response.fromDiskCache) {
      this.#responseReceivedExtraInfoEvent = undefined;
    }

    const headers = NetworkRequest.#getHeaders(
      this.#responseReceivedEvent.response.headers
    );

    return {
      method: ChromiumBidi.Network.EventNames.ResponseCompletedEvent,
      params: {
        ...this.#getBaseEventParams(),
        response: {
          url: this.#responseReceivedEvent.response.url,
          protocol: this.#responseReceivedEvent.response.protocol ?? '',
          status:
            this.#responseReceivedExtraInfoEvent?.statusCode ??
            this.#responseReceivedEvent.response.status,
          statusText: this.#responseReceivedEvent.response.statusText,
          fromCache:
            this.#responseReceivedEvent.response.fromDiskCache ||
            this.#responseReceivedEvent.response.fromPrefetchCache ||
            this.#servedFromCache,
          headers,
          mimeType: this.#responseReceivedEvent.response.mimeType,
          bytesReceived: this.#responseReceivedEvent.response.encodedDataLength,
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
    return (
      this.#requestWillBeSentEvent?.request.url.endsWith('/favicon.ico') ??
      false
    );
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
    return associatedCookies.map((cookieInfo) => {
      return {
        name: cookieInfo.cookie.name,
        value: {
          type: 'string',
          value: cookieInfo.cookie.value,
        },
        domain: cookieInfo.cookie.domain,
        path: cookieInfo.cookie.path,
        expires: cookieInfo.cookie.expires,
        size: cookieInfo.cookie.size,
        httpOnly: cookieInfo.cookie.httpOnly,
        secure: cookieInfo.cookie.secure,
        sameSite: NetworkRequest.#getCookiesSameSite(
          cookieInfo.cookie.sameSite
        ),
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
