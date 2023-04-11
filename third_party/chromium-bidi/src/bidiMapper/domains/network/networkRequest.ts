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

import Protocol from 'devtools-protocol';

import {Deferred} from '../../../utils/deferred';
import {IEventManager} from '../events/EventManager';
import {Network} from '../../../protocol/protocol';

export class NetworkRequest {
  static #unknown = 'UNKNOWN';
  requestId: string;
  #eventManager: IEventManager;
  #requestWillBeSentEvent: Protocol.Network.RequestWillBeSentEvent | undefined;
  #requestWillBeSentExtraInfoEvent:
    | Protocol.Network.RequestWillBeSentExtraInfoEvent
    | undefined;
  #responseReceivedEvent: Protocol.Network.ResponseReceivedEvent | undefined;

  #responseReceivedExtraInfoEvent:
    | Protocol.Network.ResponseReceivedExtraInfoEvent
    | undefined;

  #beforeRequestSentDeferred: Deferred<void>;
  #responseReceivedDeferred: Deferred<void>;

  constructor(requestId: string, eventManager: IEventManager) {
    this.requestId = requestId;
    this.#eventManager = eventManager;
    this.#beforeRequestSentDeferred = new Deferred<void>();
    this.#responseReceivedDeferred = new Deferred<void>();
  }

  onRequestWillBeSentEvent(
    requestWillBeSentEvent: Protocol.Network.RequestWillBeSentEvent
  ) {
    if (this.#requestWillBeSentEvent !== undefined) {
      throw new Error('RequestWillBeSentEvent is already set');
    }
    this.#requestWillBeSentEvent = requestWillBeSentEvent;
    if (this.#requestWillBeSentExtraInfoEvent !== undefined) {
      this.#beforeRequestSentDeferred.resolve();
    }
    this.#sendBeforeRequestEvent();
  }

  onRequestWillBeSentExtraInfoEvent(
    requestWillBeSentExtraInfoEvent: Protocol.Network.RequestWillBeSentExtraInfoEvent
  ) {
    if (this.#requestWillBeSentExtraInfoEvent !== undefined) {
      throw new Error('RequestWillBeSentExtraInfoEvent is already set');
    }
    this.#requestWillBeSentExtraInfoEvent = requestWillBeSentExtraInfoEvent;
    if (this.#requestWillBeSentEvent !== undefined) {
      this.#beforeRequestSentDeferred.resolve();
    }
  }

  onResponseReceivedEvent(
    responseReceivedEvent: Protocol.Network.ResponseReceivedEvent
  ) {
    if (this.#responseReceivedEvent !== undefined) {
      throw new Error('ResponseReceivedEvent is already set');
    }
    this.#responseReceivedEvent = responseReceivedEvent;
    if (this.#responseReceivedExtraInfoEvent !== undefined) {
      this.#responseReceivedDeferred.resolve();
    }
    this.#sendResponseReceivedEvent();
  }

  onResponseReceivedEventExtraInfo(
    responseReceivedExtraInfoEvent: Protocol.Network.ResponseReceivedExtraInfoEvent
  ) {
    if (this.#responseReceivedExtraInfoEvent !== undefined) {
      throw new Error('ResponseReceivedExtraInfoEvent is already set');
    }
    this.#responseReceivedExtraInfoEvent = responseReceivedExtraInfoEvent;
    if (this.#responseReceivedEvent !== undefined) {
      this.#responseReceivedDeferred.resolve();
    }
  }

  onLoadingFailedEvent(
    loadingFailedEvent: Protocol.Network.LoadingFailedEvent
  ) {
    this.#beforeRequestSentDeferred.resolve();
    this.#responseReceivedDeferred.reject(loadingFailedEvent);

    const params: Network.FetchErrorParams = {
      ...this.#getBaseEventParams(),
      errorText: loadingFailedEvent.errorText,
    };

    this.#eventManager.registerEvent(
      {
        method: Network.EventNames.FetchErrorEvent,
        params,
      },
      this.#requestWillBeSentEvent?.frameId ?? null
    );
  }

  #sendBeforeRequestEvent() {
    if (!this.#isIgnoredEvent()) {
      this.#eventManager.registerPromiseEvent(
        this.#beforeRequestSentDeferred.then(() =>
          this.#getBeforeRequestEvent()
        ),
        this.#requestWillBeSentEvent?.frameId ?? null,
        Network.EventNames.BeforeRequestSentEvent
      );
    }
  }

  #getBeforeRequestEvent(): Network.BeforeRequestSentEvent {
    if (this.#requestWillBeSentEvent === undefined) {
      throw new Error('RequestWillBeSentEvent is not set');
    }
    const params: Network.BeforeRequestSentParams = {
      ...this.#getBaseEventParams(),
      initiator: {type: this.#getInitiatorType()},
    };
    return {
      method: Network.EventNames.BeforeRequestSentEvent,
      params,
    };
  }

  #getBaseEventParams(): Network.BaseEventParams {
    return {
      context: this.#requestWillBeSentEvent?.frameId ?? null,
      navigation: this.#requestWillBeSentEvent?.loaderId ?? null,
      // TODO: implement.
      redirectCount: 0,
      request: this.#getRequestData(),
      // Timestamp should be in milliseconds, while CDP provides it in seconds.
      timestamp: Math.round(
        (this.#requestWillBeSentEvent?.wallTime ?? 0) * 1000
      ),
    };
  }

  #getRequestData(): Network.RequestData {
    const cookies =
      this.#requestWillBeSentExtraInfoEvent === undefined
        ? []
        : NetworkRequest.#getCookies(
            this.#requestWillBeSentExtraInfoEvent.associatedCookies
          );
    return {
      request:
        this.#requestWillBeSentEvent?.requestId ?? NetworkRequest.#unknown,
      url: this.#requestWillBeSentEvent?.request.url ?? NetworkRequest.#unknown,
      method:
        this.#requestWillBeSentEvent?.request.method ?? NetworkRequest.#unknown,
      headers: Object.keys(
        this.#requestWillBeSentEvent?.request.headers ?? []
      ).map((key) => ({
        name: key,
        value: this.#requestWillBeSentEvent?.request.headers[key],
      })),
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
        tlsEnd: 0,
        // TODO: implement.
        requestStart: 0,
        // TODO: implement.
        responseStart: 0,
        // TODO: implement.
        responseEnd: 0,
      },
    };
  }

  #getInitiatorType(): Network.Initiator['type'] {
    switch (this.#requestWillBeSentEvent?.initiator.type) {
      case 'parser':
      case 'script':
      case 'preflight':
        return this.#requestWillBeSentEvent?.initiator.type;
      default:
        return 'other';
    }
  }

  static #getCookiesSameSite(
    cdpSameSiteValue: string | undefined
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

  static #getCookies(
    associatedCookies: Protocol.Network.BlockedCookieWithReason[]
  ): Network.Cookie[] {
    return associatedCookies.map((cookieInfo) => {
      return {
        name: cookieInfo.cookie.name,
        value: cookieInfo.cookie.value,
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

  #sendResponseReceivedEvent() {
    if (!this.#isIgnoredEvent()) {
      // Wait for both ResponseReceived and ResponseReceivedExtraInfo events.
      this.#eventManager.registerPromiseEvent(
        this.#responseReceivedDeferred.then(() =>
          this.#getResponseReceivedEvent()
        ),
        this.#responseReceivedEvent?.frameId ?? null,
        Network.EventNames.ResponseCompletedEvent
      );
    }
  }

  #getResponseReceivedEvent(): Network.ResponseCompletedEvent {
    if (this.#responseReceivedEvent === undefined) {
      throw new Error('ResponseReceivedEvent is not set');
    }
    if (this.#requestWillBeSentEvent === undefined) {
      throw new Error('RequestWillBeSentEvent is not set');
    }

    return {
      method: Network.EventNames.ResponseCompletedEvent,
      params: {
        ...this.#getBaseEventParams(),
        response: {
          url: this.#responseReceivedEvent.response.url,
          protocol: this.#responseReceivedEvent.response.protocol,
          status: this.#responseReceivedEvent.response.status,
          statusText: this.#responseReceivedEvent.response.statusText,
          // Check if this is correct.
          fromCache:
            this.#responseReceivedEvent.response.fromDiskCache ||
            this.#responseReceivedEvent.response.fromPrefetchCache,
          // TODO: implement.
          headers: this.#getHeaders(
            this.#responseReceivedEvent.response.headers
          ),
          mimeType: this.#responseReceivedEvent.response.mimeType,
          bytesReceived: this.#responseReceivedEvent.response.encodedDataLength,
          headersSize:
            this.#responseReceivedExtraInfoEvent?.headersText?.length ?? -1,
          // TODO: consider removing from spec.
          bodySize: -1,
          content: {
            // TODO: consider removing from spec.
            size: -1,
          },
        },
      } as any as Network.ResponseCompletedParams,
    };
  }

  #getHeaders(headers: Protocol.Network.Headers) {
    return Object.keys(headers).map((key) => ({
      name: key,
      value: headers[key],
    }));
  }

  #isIgnoredEvent(): boolean {
    return (
      this.#requestWillBeSentEvent?.request.url.endsWith('/favicon.ico') ??
      false
    );
  }
}
