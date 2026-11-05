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
import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  type BrowsingContext,
  ChromiumBidi,
  Network,
  type NetworkEvent,
} from '../../../protocol/protocol.js';
import {assert} from '../../../utils/assert.js';
import {DefaultMap} from '../../../utils/DefaultMap.js';
import {Deferred} from '../../../utils/Deferred.js';
import {type LoggerFn, LogType} from '../../../utils/log.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {EventManager} from '../session/EventManager.js';

import type {NetworkStorage} from './NetworkStorage.js';
import {
  bidiBodySizeFromCdpPostDataEntries,
  bidiNetworkHeadersFromCdpNetworkHeaders,
  cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction,
  cdpFetchHeadersFromBidiNetworkHeaders,
  cdpToBiDiCookie,
  computeHeadersSize,
  getTiming,
  networkHeaderFromCookieHeaders,
  stringToBase64,
} from './NetworkUtils.js';

const REALM_REGEX = /(?<=realm=").*(?=")/;

/** Abstracts one individual network request. */
export class NetworkRequest {
  static unknownParameter = 'UNKNOWN';

  /**
   * Each network request has an associated request id, which is a string
   * uniquely identifying that request.
   *
   * The identifier for a request resulting from a redirect matches that of the
   * request that initiated it.
   */
  #id: Network.Request;

  #fetchId?: Protocol.Fetch.RequestId;

  /**
   * Indicates the network intercept phase, if the request is currently blocked.
   * Undefined necessarily implies that the request is not blocked.
   */
  #interceptPhase?: Network.InterceptPhase;

  #servedFromCache = false;

  #redirectCount: number;

  /**
   * True after a redirect response for this request hop is received. A new
   * `NetworkRequest` with the same ID represents the next hop.
   */
  #redirected = false;

  /**
   * The previous redirect hop with the same request ID. It is set only while
   * the previous hop waits for its ExtraInfo events. Events of this hop are
   * delayed until the previous hop stops waiting.
   */
  #previousHop?: NetworkRequest;
  /** The next redirect hop, which waits for this hop. */
  #nextHop?: NetworkRequest;
  /**
   * True if this redirected hop must not wait anymore for the missing ExtraInfo
   * events. For example, the next hop finished loading.
   */
  #extraInfoWaitCanceled = false;
  /**
   * The `Network.requestWillBeSent` events of the hops that follow the
   * previous hops that stopped waiting before they got their redirect
   * response ExtraInfo, in hop order. CDP can send these ExtraInfo events
   * later. The latest hop skips them. Each entry gives the URL that the late
   * ExtraInfo must redirect to.
   */
  #lateRedirectResponseExtraInfos: Protocol.Network.RequestWillBeSentEvent[] =
    [];

  #bodySize = 0;
  // Total response bytes received, updated while the response is loading.
  #responseBytesReceived = 0;
  // Encoded body size from Network.loadingFinished, or null when unavailable.
  #encodedResponseBodySize: number | null = null;
  // Tracked decoded data size while the response is loading.
  #decodedResponseBodySize = 0;

  #request: {
    info?: Protocol.Network.RequestWillBeSentEvent;
    extraInfo?: Protocol.Network.RequestWillBeSentExtraInfoEvent;
    paused?: Protocol.Fetch.RequestPausedEvent;
    auth?: Protocol.Fetch.AuthRequiredEvent;
  } = {};

  #requestOverrides?: {
    url?: string;
    method?: string;
    headers?: Network.Header[];
    cookies?: Network.CookieHeader[];
    bodySize?: number;
  };

  #responseOverrides?: {
    statusCode?: number;
    headers?: Network.Header[];
    cookies?: Network.CookieHeader[];
  };

  #response: {
    hasExtraInfo?: boolean;
    info?: Protocol.Network.Response;
    extraInfo?: Protocol.Network.ResponseReceivedExtraInfoEvent;
    paused?: Protocol.Fetch.RequestPausedEvent;
    loadingFinished?: Protocol.Network.LoadingFinishedEvent;
    loadingFailed?: Protocol.Network.LoadingFailedEvent;
  } = {};

  #eventManager: EventManager;
  #networkStorage: NetworkStorage;
  #cdpTarget: CdpTarget;
  #logger?: LoggerFn;

  #emittedEvents: Record<ChromiumBidi.Network.EventNames, boolean> = {
    [ChromiumBidi.Network.EventNames.AuthRequired]: false,
    [ChromiumBidi.Network.EventNames.BeforeRequestSent]: false,
    [ChromiumBidi.Network.EventNames.FetchError]: false,
    [ChromiumBidi.Network.EventNames.ResponseCompleted]: false,
    [ChromiumBidi.Network.EventNames.ResponseStarted]: false,
  };

  waitNextPhase: Deferred<void> = new Deferred<void>();
  // Resolved when the request is finished, used to await pending data
  // collection.
  #finished: Deferred<void> = new Deferred<void>();

  constructor(
    id: Network.Request,
    eventManager: EventManager,
    networkStorage: NetworkStorage,
    cdpTarget: CdpTarget,
    redirectCount = 0,
    logger?: LoggerFn,
    previousHop?: NetworkRequest,
  ) {
    this.#id = id;
    this.#eventManager = eventManager;
    this.#networkStorage = networkStorage;
    this.#cdpTarget = cdpTarget;
    this.#redirectCount = redirectCount;
    this.#logger = logger;
    if (previousHop) {
      // The late ExtraInfo events go to the latest hop.
      this.#lateRedirectResponseExtraInfos =
        previousHop.#lateRedirectResponseExtraInfos;
      previousHop.#lateRedirectResponseExtraInfos = [];
    }
    if (previousHop?.delaysNextHop) {
      this.#previousHop = previousHop;
      previousHop.#nextHop = this;
    }
  }

  // Tracks if the request has finished loading or failed, preventing premature
  // disposal.
  get isFinished(): boolean {
    return this.#finished.isFinished;
  }

  get finishedPromise(): Promise<void> {
    return this.#finished;
  }

  get hasResponse(): boolean {
    return Boolean(this.#response.info || this.#response.paused);
  }

  get id(): string {
    return this.#id;
  }

  get fetchId(): string | undefined {
    return this.#fetchId;
  }

  /**
   * When blocked returns the phase for it
   */
  get interceptPhase(): Network.InterceptPhase | undefined {
    return this.#interceptPhase;
  }

  get url(): string {
    // Prefer browser-emitted Fetch events over renderer-emitted Network events
    // (https://crbug.com/575090111).
    const fragment =
      (
        this.#response.paused ??
        this.#request.auth ??
        this.#request.paused ??
        this.#request.info
      )?.request.urlFragment ?? '';
    const url =
      this.#response.paused?.request.url ??
      this.#requestOverrides?.url ??
      this.#request.auth?.request.url ??
      this.#request.paused?.request.url ??
      this.#response.info?.url ??
      this.#request.info?.request.url ??
      NetworkRequest.unknownParameter;

    return `${url}${fragment}`;
  }

  get redirectCount(): number {
    return this.#redirectCount;
  }

  get cdpTarget(): CdpTarget {
    return this.#cdpTarget;
  }

  /** CdpTarget can be changed when frame is moving out of process. */
  updateCdpTarget(cdpTarget: CdpTarget): void {
    if (cdpTarget !== this.#cdpTarget) {
      this.#logger?.(LogType.debugInfo)?.(
        `Request ${this.id} was moved from ${this.#cdpTarget.id} to ${cdpTarget.id}`,
      );
      this.#cdpTarget = cdpTarget;
    }
  }

  get cdpClient(): CdpClient {
    return this.#cdpTarget.cdpClient;
  }

  isRedirecting(): boolean {
    return Boolean(this.#request.info);
  }

  /**
   * True if the next redirect hop must delay its events after the events of
   * this hop. This occurs while this hop or a previous hop waits for ExtraInfo
   * events.
   */
  get delaysNextHop(): boolean {
    return (
      this.#previousHop !== undefined ||
      this.#waitsForExtraInfo('request') ||
      this.#waitsForExtraInfo('response')
    );
  }

  /**
   * CDP can send the ExtraInfo events of a redirected hop after the
   * `Network.requestWillBeSent` event of the next hop. All hops have the same
   * request ID.
   *
   * CDP sends the ExtraInfo events of one request in hop order: request(A),
   * response(A), request(B), response(B), and so on. All of them come from
   * the same `URLLoader` in the network service through one
   * `NetworkServiceDevToolsObserver` mojo pipe, and one pipe keeps the message
   * order. `Network.requestWillBeSent` and `redirectResponse` use a different
   * path, so they can arrive before the ExtraInfo events of previous hops.
   * The lag can be longer than one hop.
   *
   * Thus, the oldest hop that waits for an ExtraInfo event of the given kind
   * gets it. If no previous hop waits for it, this hop gets it.
   */
  getHopForExtraInfo(kind: 'request' | 'response'): NetworkRequest {
    const previousHop = this.#previousHop;
    return (previousHop && previousHop.#getOldestHopWaitingFor(kind)) ?? this;
  }

  #getOldestHopWaitingFor(
    kind: 'request' | 'response',
  ): NetworkRequest | undefined {
    const previousHop = this.#previousHop;
    return (
      (previousHop && previousHop.#getOldestHopWaitingFor(kind)) ??
      (this.#waitsForExtraInfo(kind) ? this : undefined)
    );
  }

  /**
   * True if this hop is redirected and an expected ExtraInfo event of the
   * given kind is not received yet.
   */
  #waitsForExtraInfo(kind: 'request' | 'response'): boolean {
    if (
      !this.#redirected ||
      this.#extraInfoWaitCanceled ||
      // `redirectHasExtraInfo` of the next hop is false.
      !this.#response.hasExtraInfo ||
      this.#isDataUrl() ||
      this.#servedFromCache
    ) {
      return false;
    }
    return kind === 'request'
      ? !this.#request.extraInfo
      : !this.#response.extraInfo;
  }

  /**
   * Stops waiting for the missing ExtraInfo events of all previous hops and
   * emits their events. Call this method before this hop emits events that
   * do not wait for the previous hops.
   */
  #stopWaitingForPreviousHops() {
    const lateExtraInfos = this.#cancelPreviousHops();
    if (lateExtraInfos.length === 0) {
      return;
    }
    // The late ExtraInfo events go to the latest hop.
    this.#getLatestHop().#lateRedirectResponseExtraInfos.push(
      ...lateExtraInfos,
    );
  }

  /** Returns the latest linked hop of this redirect chain. */
  #getLatestHop(): NetworkRequest {
    const nextHop = this.#nextHop;
    return nextHop ? nextHop.#getLatestHop() : this;
  }

  /**
   * Cancels the ExtraInfo wait of all previous hops and emits their events.
   * Returns the `Network.requestWillBeSent` events of the hops that follow
   * the canceled hops that did not get their response ExtraInfo, in hop
   * order. CDP can still send these ExtraInfo events later.
   */
  #cancelPreviousHops(): Protocol.Network.RequestWillBeSentEvent[] {
    const previousHop = this.#previousHop;
    if (!previousHop) {
      return [];
    }
    const lateExtraInfos = previousHop.#cancelPreviousHops();
    if (previousHop.#waitsForExtraInfo('response') && this.#request.info) {
      // The late ExtraInfo of the previous hop redirects to this hop.
      lateExtraInfos.push(this.#request.info);
    }
    previousHop.#extraInfoWaitCanceled = true;
    // This call releases this hop and emits its pending events.
    previousHop.#emitEventsIfReady();
    return lateExtraInfos;
  }

  /**
   * Returns true if the event is the late redirect response ExtraInfo of a
   * previous hop that stopped waiting for it. Consumes the expected entry.
   * CDP sends the ExtraInfo events in hop order, so the late events come
   * before the ExtraInfo of the hops that still wait.
   */
  skipsLateRedirectResponseExtraInfo(
    event: Protocol.Network.ResponseReceivedExtraInfoEvent,
  ): boolean {
    const expected = this.#lateRedirectResponseExtraInfos[0];
    if (
      !expected ||
      event.statusCode < 300 ||
      event.statusCode > 399 ||
      !this.#isRedirectTo(event.headers, expected)
    ) {
      return false;
    }
    this.#lateRedirectResponseExtraInfos.shift();
    return true;
  }

  /**
   * Releases the next hop if this hop does not delay it anymore. Then emits
   * the pending events of the next hop.
   */
  #releaseNextHopIfReady() {
    const nextHop = this.#nextHop;
    if (!nextHop || this.delaysNextHop) {
      return;
    }
    this.#nextHop = undefined;
    nextHop.#previousHop = undefined;
    nextHop.#emitEventsIfReady();
  }

  #isDataUrl(): boolean {
    return this.url.startsWith('data:');
  }

  #isNonInterceptable(): boolean {
    return (
      // We can't intercept data urls from CDP
      this.#isDataUrl() ||
      // Cached requests never hit the network
      this.#servedFromCache
    );
  }

  get #method(): string | undefined {
    // Prefer browser-emitted Fetch events over renderer-emitted Network events
    // (https://crbug.com/575090111).
    return (
      this.#response.paused?.request.method ??
      this.#requestOverrides?.method ??
      this.#request.auth?.request.method ??
      this.#request.paused?.request.method ??
      this.#request.info?.request.method
    );
  }

  get #navigationId(): BrowsingContext.Navigation | null {
    // Heuristic to determine if this is a navigation request, and if not return null.
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

    // Get virtual navigation ID from the browsing context.
    return this.#networkStorage.getNavigationId(this.#context ?? undefined);
  }

  get #cookies() {
    let cookies: Network.Cookie[] = [];
    if (this.#request.extraInfo) {
      cookies = this.#request.extraInfo.associatedCookies
        .filter(({blockedReasons}) => {
          return !Array.isArray(blockedReasons) || blockedReasons.length === 0;
        })
        .map(({cookie}) => cdpToBiDiCookie(cookie));
    }
    return cookies;
  }

  #getBodySizeFromHeaders(
    headers: Protocol.Network.Headers | undefined,
  ): number | undefined {
    if (headers === undefined) {
      return undefined;
    }

    if (headers['Content-Length'] !== undefined) {
      const bodySize = Number.parseInt(headers['Content-Length']);
      if (Number.isInteger(bodySize)) {
        return bodySize;
      }
      this.#logger?.(LogType.debugError)?.(
        "Unexpected non-integer 'Content-Length' header",
      );
    }

    // TODO: process `Transfer-Encoding: chunked` case properly.

    return undefined;
  }

  #updateBodySize() {
    if (typeof this.#requestOverrides?.bodySize === 'number') {
      this.#bodySize = this.#requestOverrides.bodySize;
      return;
    }
    if (this.#request.info?.request.postDataEntries !== undefined) {
      this.#bodySize = bidiBodySizeFromCdpPostDataEntries(
        this.#request.info?.request.postDataEntries,
      );
      return;
    }

    this.#bodySize =
      this.#getBodySizeFromHeaders(this.#request.info?.request.headers) ??
      this.#getBodySizeFromHeaders(this.#request.extraInfo?.headers) ??
      0;
  }

  get bodySize(): number {
    return this.#bodySize;
  }

  get #context(): string | null {
    // Prefer browser-emitted Fetch events over renderer-emitted Network events
    // (https://crbug.com/575090111).
    const result =
      this.#response.paused?.frameId ??
      this.#request.auth?.frameId ??
      this.#request.paused?.frameId ??
      this.#request.info?.frameId;

    if (result !== undefined) {
      return result;
    }

    // Heuristic for associating a preflight request with context via it's initiator
    // request. Useful for preflight requests.
    // https://github.com/GoogleChromeLabs/chromium-bidi/issues/3570
    if (
      this.#request?.info?.initiator.type === 'preflight' &&
      this.#request?.info?.initiator.requestId !== undefined
    ) {
      const maybeInitiator = this.#networkStorage.getRequestById(
        this.#request?.info?.initiator.requestId,
      );
      if (maybeInitiator !== undefined) {
        return maybeInitiator.#context;
      }
    }

    return null;
  }

  /** Returns the HTTP status code associated with this request if any. */
  get #statusCode(): number | undefined {
    return (
      this.#responseOverrides?.statusCode ??
      this.#response.paused?.responseStatusCode ??
      this.#response.extraInfo?.statusCode ??
      this.#response.info?.status
    );
  }

  get #requestHeaders(): Network.Header[] {
    let headers: Network.Header[] = [];
    if (this.#requestOverrides?.headers) {
      const headerMap = new DefaultMap<string, string[]>(() => []);
      for (const header of this.#requestOverrides.headers) {
        headerMap.get(header.name).push(header.value.value);
      }
      for (const [name, value] of headerMap.entries()) {
        headers.push({
          name,
          value: {
            type: 'string',
            value: value.join('\n').trimEnd(),
          },
        });
      }
    } else {
      // Prefer browser-emitted Fetch request headers over renderer-emitted
      // Network.requestWillBeSent headers when available
      // (https://crbug.com/575090111).
      const requestHeaders =
        this.#response.paused?.request.headers ??
        this.#request.auth?.request.headers ??
        this.#request.paused?.request.headers ??
        this.#request.info?.request.headers;
      headers = bidiNetworkHeadersFromCdpNetworkHeaders({
        ...requestHeaders,
        ...this.#request.extraInfo?.headers,
      });
    }

    return headers;
  }

  get #authChallenges(): Network.AuthChallenge[] | undefined {
    // TODO: get headers from Fetch.requestPaused
    if (!this.#response.info) {
      return;
    }

    if (!(this.#statusCode === 401 || this.#statusCode === 407)) {
      return undefined;
    }

    const headerName =
      this.#statusCode === 401 ? 'WWW-Authenticate' : 'Proxy-Authenticate';

    const authChallenges = [];
    for (const [header, value] of Object.entries(this.#response.info.headers)) {
      // TODO: Do a proper match based on https://httpwg.org/specs/rfc9110.html#credentials
      // Or verify this works
      if (
        header.localeCompare(headerName, undefined, {sensitivity: 'base'}) === 0
      ) {
        authChallenges.push({
          scheme: value.split(' ').at(0) ?? '',
          realm: value.match(REALM_REGEX)?.at(0) ?? '',
        });
      }
    }

    return authChallenges;
  }

  get #timings(): Network.FetchTimingInfo {
    // The timing in the CDP events are provided relative to the event's baseline.
    // However, the baseline can be different for different events, and the events have to
    // be normalized throughout resource events. Normalize events timestamps  by the
    // request.
    // TODO: Verify this is correct.
    const responseTimeOffset = getTiming(
      getTiming(this.#response.info?.timing?.requestTime) -
        getTiming(this.#request.info?.timestamp),
    );

    return {
      // TODO: Verify this is correct
      timeOrigin: Math.round(getTiming(this.#request.info?.wallTime) * 1000),
      // Timing baseline.
      // TODO: Verify this is correct.
      requestTime: 0,
      // TODO: set if redirect detected.
      redirectStart: 0,
      // TODO: set if redirect detected.
      redirectEnd: 0,
      // TODO: Verify this is correct
      // https://source.chromium.org/chromium/chromium/src/+/main:net/base/load_timing_info.h;l=145
      fetchStart: getTiming(
        this.#response.info?.timing?.workerFetchStart,
        responseTimeOffset,
      ),
      // fetchStart: 0,
      dnsStart: getTiming(
        this.#response.info?.timing?.dnsStart,
        responseTimeOffset,
      ),
      dnsEnd: getTiming(
        this.#response.info?.timing?.dnsEnd,
        responseTimeOffset,
      ),
      connectStart: getTiming(
        this.#response.info?.timing?.connectStart,
        responseTimeOffset,
      ),
      connectEnd: getTiming(
        this.#response.info?.timing?.connectEnd,
        responseTimeOffset,
      ),
      tlsStart: getTiming(
        this.#response.info?.timing?.sslStart,
        responseTimeOffset,
      ),
      requestStart: getTiming(
        this.#response.info?.timing?.sendStart,
        responseTimeOffset,
      ),
      // https://source.chromium.org/chromium/chromium/src/+/main:net/base/load_timing_info.h;l=196
      responseStart: getTiming(
        this.#response.info?.timing?.receiveHeadersStart,
        responseTimeOffset,
      ),
      responseEnd: getTiming(
        this.#response.info?.timing?.receiveHeadersEnd,
        responseTimeOffset,
      ),
    };
  }

  #phaseChanged() {
    this.waitNextPhase.resolve();
    this.waitNextPhase = new Deferred();
  }

  #interceptsInPhase(phase: Network.InterceptPhase) {
    if (
      this.#isNonInterceptable() ||
      !this.#cdpTarget.isSubscribedTo(`network.${phase}`)
    ) {
      return new Set();
    }

    return this.#networkStorage.getInterceptsForPhase(this, phase);
  }

  #isBlockedInPhase(phase: Network.InterceptPhase) {
    return this.#interceptsInPhase(phase).size > 0;
  }

  handleRedirect(event: Protocol.Network.RequestWillBeSentEvent): void {
    this.#finished.resolve();
    this.#redirected = true;
    // `redirectHasExtraInfo` tells if CDP sends ExtraInfo events for the
    // redirected hop. These events can arrive after this event.
    this.#response.hasExtraInfo = event.redirectHasExtraInfo;
    this.#decodedResponseBodySize = 0;
    // Redirect response bodies are never read, so their encoded size is 0.
    this.#encodedResponseBodySize = 0;
    this.#responseBytesReceived = event.redirectResponse!.encodedDataLength;
    this.#response.info = event.redirectResponse!;
    this.#emitEventsIfReady();
  }

  #emitEventsIfReady() {
    if (this.#previousHop) {
      // Keep the BiDi event order. The previous hop emits its events first.
      return;
    }

    const requestExtraInfoCompleted =
      Boolean(this.#response.loadingFailed) ||
      // The response is paused in Fetch, so the request stage is over. Do not
      // emit `network.responseStarted` before `network.beforeRequestSent`.
      Boolean(this.#response.paused) ||
      this.#isDataUrl() ||
      Boolean(this.#request.extraInfo) ||
      // The next hop finished, but this redirected hop has no ExtraInfo.
      this.#extraInfoWaitCanceled ||
      // If the request is intercepted during the `authRequired` phase, there
      // will be no `Network.requestWillBeSentExtraInfo` CDP events.
      this.#isBlockedInPhase(Network.InterceptPhase.AuthRequired) ||
      // Requests from cache don't have extra info
      this.#servedFromCache ||
      // Sometimes there is no extra info and the response
      // is the only place we can find out
      Boolean(this.#response.info && !this.#response.hasExtraInfo);

    const noInterceptionExpected = this.#isNonInterceptable();

    const requestInterceptionExpected =
      !noInterceptionExpected &&
      this.#isBlockedInPhase(Network.InterceptPhase.BeforeRequestSent);

    const requestInterceptionCompleted =
      !requestInterceptionExpected ||
      (requestInterceptionExpected && Boolean(this.#request.paused));

    const beforeRequestSentReady =
      Boolean(this.#request.info) &&
      (requestInterceptionExpected
        ? requestInterceptionCompleted
        : requestExtraInfoCompleted);

    if (beforeRequestSentReady) {
      this.#emitEvent(this.#getBeforeRequestEvent.bind(this));
    }
    // Do not emit response events before `network.beforeRequestSent`. Keep
    // this value true after `network.beforeRequestSent` is emitted, because
    // `beforeRequestSentReady` can change when the intercepts change.
    const requestPhaseCompleted =
      beforeRequestSentReady ||
      this.#emittedEvents[ChromiumBidi.Network.EventNames.BeforeRequestSent] ||
      !this.#request.info;

    const responseExtraInfoCompleted =
      Boolean(this.#response.extraInfo) ||
      this.#isDataUrl() ||
      // The next hop finished, but this redirected hop has no ExtraInfo.
      this.#extraInfoWaitCanceled ||
      // Response from cache don't have extra info
      this.#servedFromCache ||
      // Don't expect extra info if the flag is false
      Boolean(this.#response.info && !this.#response.hasExtraInfo);

    const responseInterceptionExpected =
      !noInterceptionExpected &&
      this.#isBlockedInPhase(Network.InterceptPhase.ResponseStarted);

    if (
      (this.#response.info && requestPhaseCompleted) ||
      (responseInterceptionExpected && Boolean(this.#response.paused))
    ) {
      this.#emitEvent(this.#getResponseStartedEvent.bind(this));
    }

    const responseInterceptionCompleted =
      !responseInterceptionExpected ||
      (responseInterceptionExpected && Boolean(this.#response.paused));

    const loadingFinished =
      Boolean(this.#response.loadingFailed) ||
      Boolean(this.#response.loadingFinished);

    if (
      Boolean(this.#response.info) &&
      requestPhaseCompleted &&
      responseExtraInfoCompleted &&
      responseInterceptionCompleted &&
      (loadingFinished || this.#redirected)
    ) {
      this.#emitEvent(this.#getResponseReceivedEvent.bind(this));
      if (!this.#response.loadingFailed) {
        if (this.#response.loadingFinished) {
          this.#networkStorage.collectIfNeeded(this, Network.DataType.Response);
        }
        // Resolve `#finished` after `collectIfNeeded` and `ResponseCompleted`
        // so `getData` sees updated size-limit state and `disposeRequest` does
        // not wipe state before `ResponseCompleted` is emitted.
        this.#finished.resolve();
        if (!this.#redirected) {
          // The storage disposes redirected hops. The next hop can have the
          // same ID in the storage.
          this.#networkStorage.disposeRequest(this.id);
        }
      }
    }

    this.#releaseNextHopIfReady();
  }

  onRequestWillBeSentEvent(
    event: Protocol.Network.RequestWillBeSentEvent,
  ): void {
    this.#request.info = event;
    this.#updateBodySize();
    this.#networkStorage.collectIfNeeded(this, Network.DataType.Request);
    this.#emitEventsIfReady();
  }

  onRequestWillBeSentExtraInfoEvent(
    event: Protocol.Network.RequestWillBeSentExtraInfoEvent,
  ): void {
    this.#request.extraInfo = event;
    this.#updateBodySize();
    this.#emitEventsIfReady();
  }

  onResponseReceivedExtraInfoEvent(
    event: Protocol.Network.ResponseReceivedExtraInfoEvent,
  ): void {
    this.#response.extraInfo = event;
    this.#emitEventsIfReady();
  }

  /**
   * Returns true if the `location` header points to the URL of this request.
   * The header can contain a relative URL.
   */
  #isRedirectTo(
    headers: Protocol.Network.Headers,
    info: Protocol.Network.RequestWillBeSentEvent,
  ): boolean {
    const location = Object.entries(headers).find(
      ([name]) => name.toLowerCase() === 'location',
    )?.[1];
    if (location === undefined) {
      return false;
    }
    if (location === info.request.url) {
      return true;
    }
    try {
      // The redirect response URL is the URL of the previous hop.
      const base = info.redirectResponse?.url ?? info.request.url;
      const target = new URL(location, base);
      // CDP sends the URL fragment separately in `request.urlFragment`.
      target.hash = '';
      return target.href === info.request.url;
    } catch {
      return false;
    }
  }

  onResponseReceivedEvent(event: Protocol.Network.ResponseReceivedEvent): void {
    this.#response.hasExtraInfo = event.hasExtraInfo;
    this.#response.info = event.response;
    this.#responseBytesReceived = event.response.encodedDataLength;
    this.#emitEventsIfReady();
  }

  onServedFromCache(): void {
    this.#servedFromCache = true;
    this.#emitEventsIfReady();
  }

  onLoadingFinishedEvent(event: Protocol.Network.LoadingFinishedEvent): void {
    this.#response.loadingFinished = event;
    this.#responseBytesReceived = event.encodedDataLength;
    this.#encodedResponseBodySize = event.encodedBodyLength ?? null;
    // The missing ExtraInfo events of the previous hops do not arrive anymore.
    // This call can emit the events of this hop, so set the fields first.
    this.#stopWaitingForPreviousHops();
    this.#emitEventsIfReady();
  }

  onDataReceivedEvent(event: Protocol.Network.DataReceivedEvent): void {
    this.#decodedResponseBodySize += event.dataLength;
    this.#responseBytesReceived += event.encodedDataLength;
  }

  onLoadingFailedEvent(event: Protocol.Network.LoadingFailedEvent): void {
    this.#response.loadingFailed = event;
    // The missing ExtraInfo events of the previous hops do not arrive anymore.
    // This call can emit the events of this hop, so set the field first.
    this.#stopWaitingForPreviousHops();
    this.#emitEventsIfReady();

    this.#emitEvent(() => {
      return {
        method: ChromiumBidi.Network.EventNames.FetchError,
        params: {
          ...this.#getBaseEventParams(),
          errorText: event.errorText,
        },
      };
    });
    this.#finished.resolve();
    this.#networkStorage.disposeRequest(this.id);
  }

  /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-failRequest */
  async failRequest(errorReason: Protocol.Network.ErrorReason): Promise<void> {
    assert(this.#fetchId, 'Network Interception not set-up.');

    await this.cdpClient.sendCommand('Fetch.failRequest', {
      requestId: this.#fetchId,
      errorReason,
    });
    this.#interceptPhase = undefined;
  }

  onRequestPaused(event: Protocol.Fetch.RequestPausedEvent): void {
    this.#fetchId = event.requestId;

    // CDP https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#event-requestPaused
    if (event.responseStatusCode || event.responseErrorReason) {
      this.#response.paused = event;

      if (
        this.#isBlockedInPhase(Network.InterceptPhase.ResponseStarted) &&
        // CDP may emit multiple events for a single request
        !this.#emittedEvents[ChromiumBidi.Network.EventNames.ResponseStarted] &&
        // Continue all response that have not enabled Network domain
        this.#fetchId !== this.id
      ) {
        this.#interceptPhase = Network.InterceptPhase.ResponseStarted;
        // Do not delay the blocked request. An auto-continued request keeps
        // waiting, so that the late ExtraInfo of the previous hops is not
        // applied to this hop.
        // TODO: Count the missing request ExtraInfo of the canceled hops and
        // skip it on the latest hop, like the response ExtraInfo.
        this.#stopWaitingForPreviousHops();
      } else {
        void this.#continueResponse().catch((error: unknown) => {
          this.#logger?.(LogType.debugError)?.(error);
        });
      }
    } else {
      this.#request.paused = event;
      if (
        this.#isBlockedInPhase(Network.InterceptPhase.BeforeRequestSent) &&
        // CDP may emit multiple events for a single request
        !this.#emittedEvents[
          ChromiumBidi.Network.EventNames.BeforeRequestSent
        ] &&
        // Continue all requests that have not enabled Network domain
        this.#fetchId !== this.id
      ) {
        this.#interceptPhase = Network.InterceptPhase.BeforeRequestSent;
        // Do not delay the blocked request. See the comment above.
        this.#stopWaitingForPreviousHops();
      } else {
        void this.#continueRequest().catch((error: unknown) => {
          this.#logger?.(LogType.debugError)?.(error);
        });
      }
    }

    this.#emitEventsIfReady();
  }

  onAuthRequired(event: Protocol.Fetch.AuthRequiredEvent): void {
    this.#fetchId = event.requestId;
    this.#request.auth = event;
    // Do not delay the request. This method always emits
    // `network.authRequired`. This call can emit the events of this hop, so
    // set the fields first.
    this.#stopWaitingForPreviousHops();

    if (
      this.#isBlockedInPhase(Network.InterceptPhase.AuthRequired) &&
      // Continue all auth requests that have not enabled Network domain
      this.#fetchId !== this.id
    ) {
      this.#interceptPhase = Network.InterceptPhase.AuthRequired;
      // Make sure the `network.beforeRequestSent` is emitted before
      // `network.authRequired`.
      this.#emitEventsIfReady();
    } else {
      void this.#continueWithAuth({
        response: 'Default',
      }).catch((error: unknown) => {
        this.#logger?.(LogType.debugError)?.(error);
      });
    }

    this.#emitEvent(() => {
      return {
        method: ChromiumBidi.Network.EventNames.AuthRequired,
        params: {
          ...this.#getBaseEventParams(Network.InterceptPhase.AuthRequired),
          response: this.#getResponseEventParams(),
        },
      };
    });
  }

  /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueRequest */
  async continueRequest(
    overrides: Omit<Network.ContinueRequestParameters, 'request'> = {},
  ): Promise<void> {
    const overrideHeaders = this.#getOverrideHeader(
      overrides.headers,
      overrides.cookies,
    );
    const headers = cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);
    const postData = getCdpBodyFromBiDiBytesValue(overrides.body);

    await this.#continueRequest({
      url: overrides.url,
      method: overrides.method,
      headers,
      postData,
    });

    this.#requestOverrides = {
      url: overrides.url,
      method: overrides.method,
      headers: overrides.headers,
      cookies: overrides.cookies,
      bodySize: getSizeFromBiDiBytesValue(overrides.body),
    };
  }

  async #continueRequest(
    overrides: Omit<Protocol.Fetch.ContinueRequestRequest, 'requestId'> = {},
  ) {
    assert(this.#fetchId, 'Network Interception not set-up.');

    await this.cdpClient.sendCommand('Fetch.continueRequest', {
      requestId: this.#fetchId,
      url: overrides.url,
      method: overrides.method,
      headers: overrides.headers,
      postData: overrides.postData,
    });

    this.#interceptPhase = undefined;
  }

  /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueResponse */
  async continueResponse(
    overrides: Omit<Network.ContinueResponseParameters, 'request'> = {},
  ): Promise<void> {
    if (this.interceptPhase === Network.InterceptPhase.AuthRequired) {
      if (overrides.credentials) {
        await Promise.all([
          this.waitNextPhase,
          await this.#continueWithAuth({
            response: 'ProvideCredentials',
            username: overrides.credentials.username,
            password: overrides.credentials.password,
          }),
        ]);
      } else {
        // We need to use `ProvideCredentials`
        // As `Default` may cancel the request
        return await this.#continueWithAuth({
          response: 'ProvideCredentials',
        });
      }
    }

    if (this.#interceptPhase === Network.InterceptPhase.ResponseStarted) {
      const overrideHeaders = this.#getOverrideHeader(
        overrides.headers,
        overrides.cookies,
      );
      const responseHeaders =
        cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);

      await this.#continueResponse({
        responseCode:
          overrides.statusCode ?? this.#response.paused?.responseStatusCode,
        responsePhrase:
          overrides.reasonPhrase ?? this.#response.paused?.responseStatusText,
        responseHeaders:
          responseHeaders ?? this.#response.paused?.responseHeaders,
      });

      this.#responseOverrides = {
        statusCode: overrides.statusCode,
        headers: overrideHeaders,
      };
    }
  }

  async #continueResponse({
    responseCode,
    responsePhrase,
    responseHeaders,
  }: Omit<Protocol.Fetch.ContinueResponseRequest, 'requestId'> = {}) {
    assert(this.#fetchId, 'Network Interception not set-up.');

    await this.cdpClient.sendCommand('Fetch.continueResponse', {
      requestId: this.#fetchId,
      responseCode,
      responsePhrase,
      responseHeaders,
    });
    this.#interceptPhase = undefined;
  }

  /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-continueWithAuth */
  async continueWithAuth(
    authChallenge: Omit<Network.ContinueWithAuthParameters, 'request'>,
  ): Promise<void> {
    let username: string | undefined;
    let password: string | undefined;

    if (authChallenge.action === 'provideCredentials') {
      const {credentials} =
        authChallenge as Network.ContinueWithAuthCredentials;

      username = credentials.username;
      password = credentials.password;
    }

    const response = cdpAuthChallengeResponseFromBidiAuthContinueWithAuthAction(
      authChallenge.action,
    );

    await this.#continueWithAuth({
      response,
      username,
      password,
    });
  }

  /** @see https://chromedevtools.github.io/devtools-protocol/tot/Fetch/#method-provideResponse */
  async provideResponse(
    overrides: Omit<Network.ProvideResponseParameters, 'request'>,
  ): Promise<void> {
    assert(this.#fetchId, 'Network Interception not set-up.');

    // We need to pass through if the request is already in
    // AuthRequired phase
    if (this.interceptPhase === Network.InterceptPhase.AuthRequired) {
      // We need to use `ProvideCredentials`
      // As `Default` may cancel the request
      return await this.#continueWithAuth({
        response: 'ProvideCredentials',
      });
    }

    const overrideHeaders = this.#getOverrideHeader(
      overrides.headers,
      overrides.cookies,
    );
    const responseHeaders =
      cdpFetchHeadersFromBidiNetworkHeaders(overrideHeaders);

    const responseCode = overrides.statusCode ?? this.#statusCode ?? 200;

    await this.cdpClient.sendCommand('Fetch.fulfillRequest', {
      requestId: this.#fetchId,
      responseCode,
      responsePhrase: overrides.reasonPhrase,
      responseHeaders,
      body: getCdpBodyFromBiDiBytesValue(overrides.body),
    });
    this.#interceptPhase = undefined;
  }

  dispose(): void {
    // Unblock any pending `getData` calls when the target detaches.
    this.#finished.resolve();
    this.waitNextPhase.reject(new Error('waitNextPhase disposed'));
  }

  disposeData(): void {
    this.#request = {};
    this.#response = {};
    this.#requestOverrides = undefined;
    this.#responseOverrides = undefined;
  }

  async #continueWithAuth(
    authChallengeResponse: Protocol.Fetch.ContinueWithAuthRequest['authChallengeResponse'],
  ) {
    assert(this.#fetchId, 'Network Interception not set-up.');

    await this.cdpClient.sendCommand('Fetch.continueWithAuth', {
      requestId: this.#fetchId,
      authChallengeResponse,
    });
    this.#interceptPhase = undefined;
  }

  #emitEvent(getEvent: () => NetworkEvent) {
    let event: NetworkEvent;
    try {
      event = getEvent();
    } catch (error) {
      this.#logger?.(LogType.debugError)?.(error);
      return;
    }

    if (
      this.#isIgnoredEvent() ||
      (this.#emittedEvents[event.method] &&
        // Special case this event can be emitted multiple times
        event.method !== ChromiumBidi.Network.EventNames.AuthRequired)
    ) {
      return;
    }
    this.#phaseChanged();

    this.#emittedEvents[event.method] = true;
    if (this.#context) {
      this.#eventManager.registerPromiseEvent(
        this.#resolveEvent(event),
        this.#context,
        event.method,
      );
    } else {
      this.#eventManager.registerGlobalPromiseEvent(
        this.#resolveEvent(event),
        event.method,
        this.#cdpTarget.userContext,
      );
    }
  }

  /**
   * Resolves a deferred event after asynchronously gathering any missing event
   * data, while preserving the event's position in the outgoing queue.
   */
  async #resolveEvent(event: NetworkEvent) {
    const paused = this.#request.paused ?? this.#response.paused;
    if (!this.#request.extraInfo && paused) {
      // Workaround for CDP omitting `Network.requestWillBeSentExtraInfo` (which
      // provides `associatedCookies`) while a request is paused in `Fetch`.
      event.params.request.cookies =
        await this.#fetchPausedRequestCookies(paused);
    }
    return {
      kind: 'success' as const,
      value: Object.assign(event, {
        type: 'event' as const,
      }),
    };
  }

  /** Queries CDP for cookies matching the paused request's `Cookie` header. */
  async #fetchPausedRequestCookies(
    event: Protocol.Fetch.RequestPausedEvent,
  ): Promise<Network.Cookie[]> {
    const cookieHeader = Object.entries(event.request.headers).find(
      ([name]) => name.toLowerCase() === 'cookie',
    )?.[1];
    if (!cookieHeader) {
      return [];
    }

    const cookiePairs = new Set(
      cookieHeader.split(';').map((pair) => pair.trim()),
    );
    try {
      const result = await this.cdpClient.sendCommand('Network.getCookies', {
        urls: [event.request.url],
      });
      return (result?.cookies ?? [])
        .filter((cookie) =>
          cookiePairs.has(
            cookie.name === ''
              ? cookie.value
              : `${cookie.name}=${cookie.value}`,
          ),
        )
        .map((cookie) => cdpToBiDiCookie(cookie));
    } catch {
      return [];
    }
  }

  #getBaseEventParams(phase?: Network.InterceptPhase): Network.BaseParameters {
    const interceptProps: Pick<
      Network.BaseParameters,
      'isBlocked' | 'intercepts'
    > = {
      isBlocked: false,
    };

    if (phase) {
      const blockedBy = this.#interceptsInPhase(phase);
      interceptProps.isBlocked = blockedBy.size > 0;
      if (interceptProps.isBlocked) {
        interceptProps.intercepts = [...blockedBy] as [
          Network.Intercept,
          ...Network.Intercept[],
        ];
      }
    }

    return {
      context: this.#context,
      navigation: this.#navigationId,
      redirectCount: this.#redirectCount,
      request: this.#getRequestData(),
      // Timestamp should be in milliseconds, while CDP provides it in seconds.
      timestamp: Math.round(getTiming(this.#request.info?.wallTime) * 1000),
      userContext: this.#cdpTarget.userContext,
      // Contains isBlocked and intercepts
      ...interceptProps,
    };
  }

  #getResponseEventParams(): Network.ResponseData {
    // Chromium sends wrong extraInfo events for responses served from cache.
    // See https://github.com/puppeteer/puppeteer/issues/9965 and
    // https://crbug.com/1340398.
    if (this.#response.info?.fromDiskCache) {
      this.#response.extraInfo = undefined;
    }

    // TODO: Also this.#response.paused?.responseHeaders have to be merged here.
    const cdpHeaders = this.#response.info?.headers ?? {};
    const cdpRawHeaders = this.#response.extraInfo?.headers ?? {};
    for (const [key, value] of Object.entries(cdpRawHeaders)) {
      cdpHeaders[key] = value;
    }
    const headers = bidiNetworkHeadersFromCdpNetworkHeaders(cdpHeaders);
    const authChallenges = this.#authChallenges;

    const response: Network.ResponseData = {
      url: this.url,
      protocol: this.#response.info?.protocol ?? '',
      status: this.#statusCode ?? -1, // TODO: Throw an exception or use some other status code?
      statusText:
        this.#response.info?.statusText ||
        this.#response.paused?.responseStatusText ||
        '',
      fromCache:
        this.#response.info?.fromDiskCache ||
        this.#response.info?.fromPrefetchCache ||
        this.#servedFromCache,
      headers: this.#responseOverrides?.headers ?? headers,
      mimeType: this.#response.info?.mimeType || '',
      bytesReceived: this.responseBytesReceived,
      headersSize: computeHeadersSize(headers),
      bodySize: this.encodedResponseBodySize,
      content: {
        size: this.#decodedResponseBodySize,
      },
      ...(authChallenges ? {authChallenges} : {}),
    };

    return {
      ...response,
      'goog:securityDetails': this.#response.info?.securityDetails,
    } as Network.ResponseData;
  }

  get encodedResponseBodySize(): number | null {
    return this.#encodedResponseBodySize;
  }

  get responseBytesReceived(): number {
    return this.#responseBytesReceived;
  }

  get decodedResponseBodySize(): number {
    return this.#decodedResponseBodySize;
  }

  #getRequestData(): Network.RequestData {
    const headers = this.#requestHeaders;

    const request: Network.RequestData = {
      request: this.#id,
      url: this.url,
      method: this.#method ?? NetworkRequest.unknownParameter,
      headers,
      cookies: this.#cookies,
      headersSize: computeHeadersSize(headers),
      bodySize: this.bodySize,
      // TODO: populate
      destination: this.#getDestination(),
      // TODO: populate
      initiatorType: this.#getInitiatorType(),
      timings: this.#timings,
    };

    return {
      ...request,
      'goog:postData': this.#request.info?.request?.postData,
      'goog:hasPostData': this.#request.info?.request?.hasPostData,
      'goog:resourceType': this.#request.info?.type,
      'goog:resourceInitiator': this.#request.info?.initiator,
    } as Network.RequestData;
  }

  /**
   * Heuristic trying to guess the destination.
   * Specification: https://fetch.spec.whatwg.org/#concept-request-destination.
   * Specified values: "audio", "audioworklet", "document", "embed", "font", "frame",
   * "iframe", "image", "json", "manifest", "object", "paintworklet", "report", "script",
   * "serviceworker", "sharedworker", "style", "track", "video", "webidentity", "worker",
   * "xslt".
   */
  #getDestination(): string {
    switch (this.#request.info?.type) {
      case 'Script':
        return 'script';
      case 'Stylesheet':
        return 'style';
      case 'Image':
        return 'image';
      case 'Document':
        // If request to document is initiated by parser, assume it is expected to
        // arrive in an iframe. Otherwise, consider it is a navigation and the request
        // result will end up in the document.
        return this.#request.info?.initiator.type === 'parser'
          ? 'iframe'
          : 'document';
      default:
        return '';
    }
  }

  /**
   * Heuristic trying to guess the initiator type.
   * Specification: https://fetch.spec.whatwg.org/#request-initiator-type.
   * Specified values: "audio", "beacon", "body", "css", "early-hints", "embed", "fetch",
   * "font", "frame", "iframe", "image", "img", "input", "link", "object", "ping",
   * "script", "track", "video", "xmlhttprequest", "other".
   */
  #getInitiatorType(): null | string {
    if (this.#request.info?.initiator.type === 'parser') {
      switch (this.#request.info?.type) {
        case 'Document':
          // The request to document is initiated by the parser. Assuming it's an iframe.
          return 'iframe';
        case 'Font':
          // If the document's url is not the parser's url, assume the resource is loaded
          // from css. Otherwise, it's a `font` element.
          return this.#request.info?.initiator?.url ===
            this.#request.info?.documentURL
            ? 'font'
            : 'css';
        case 'Image':
          // If the document's url is not the parser's url, assume the resource is loaded
          // from css. Otherwise, it's a `img` element.
          return this.#request.info?.initiator?.url ===
            this.#request.info?.documentURL
            ? 'img'
            : 'css';
        case 'Script':
          return 'script';
        case 'Stylesheet':
          return 'link';
        default:
          return null;
      }
    }
    if (this.#request?.info?.type === 'Fetch') {
      return 'fetch';
    }

    return null;
  }

  #getBeforeRequestEvent(): Network.BeforeRequestSent {
    assert(this.#request.info, 'RequestWillBeSentEvent is not set');

    return {
      method: ChromiumBidi.Network.EventNames.BeforeRequestSent,
      params: {
        ...this.#getBaseEventParams(Network.InterceptPhase.BeforeRequestSent),
        initiator: {
          type: NetworkRequest.#getInitiator(this.#request.info.initiator.type),
          columnNumber: this.#request.info.initiator.columnNumber,
          lineNumber: this.#request.info.initiator.lineNumber,
          stackTrace: this.#request.info.initiator.stack,
          request: this.#request.info.initiator.requestId,
        },
      },
    };
  }

  #getResponseStartedEvent(): Network.ResponseStarted {
    return {
      method: ChromiumBidi.Network.EventNames.ResponseStarted,
      params: {
        ...this.#getBaseEventParams(Network.InterceptPhase.ResponseStarted),
        response: this.#getResponseEventParams(),
      },
    };
  }

  #getResponseReceivedEvent(): Network.ResponseCompleted {
    return {
      method: ChromiumBidi.Network.EventNames.ResponseCompleted,
      params: {
        ...this.#getBaseEventParams(),
        response: this.#getResponseEventParams(),
      },
    };
  }

  #isIgnoredEvent(): boolean {
    const faviconUrl = '/favicon.ico';
    return (
      this.#request.paused?.request.url.endsWith(faviconUrl) ??
      this.#request.info?.request.url.endsWith(faviconUrl) ??
      false
    );
  }

  #getOverrideHeader(
    headers: Network.Header[] | undefined,
    cookies: Network.CookieHeader[] | undefined,
  ): Network.Header[] | undefined {
    if (!headers && !cookies) {
      return undefined;
    }
    let overrideHeaders: Network.Header[] | undefined = headers;
    const cookieHeader = networkHeaderFromCookieHeaders(cookies);
    if (cookieHeader && !overrideHeaders) {
      overrideHeaders = this.#requestHeaders;
    }
    if (cookieHeader && overrideHeaders) {
      overrideHeaders.filter(
        (header) =>
          header.name.localeCompare('cookie', undefined, {
            sensitivity: 'base',
          }) !== 0,
      );
      overrideHeaders.push(cookieHeader);
    }

    return overrideHeaders;
  }

  static #getInitiator(
    initiatorType: Protocol.Network.Initiator['type'],
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
}

function getCdpBodyFromBiDiBytesValue(
  body?: Network.BytesValue,
): string | undefined {
  let parsedBody: string | undefined;
  if (body?.type === 'string') {
    parsedBody = stringToBase64(body.value);
  } else if (body?.type === 'base64') {
    parsedBody = body.value;
  }
  return parsedBody;
}

function getSizeFromBiDiBytesValue(body?: Network.BytesValue): number {
  if (body?.type === 'string') {
    return body.value.length;
  } else if (body?.type === 'base64') {
    return atob(body.value).length;
  }
  return 0;
}
