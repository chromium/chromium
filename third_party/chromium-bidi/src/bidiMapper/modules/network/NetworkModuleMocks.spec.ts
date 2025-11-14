/*
 * Copyright 2024 Google LLC.
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
import type {Protocol} from 'devtools-protocol';

import {
  CloseError,
  type CdpClient,
  type CdpEvents,
} from '../../../cdp/CdpClient.js';
import {EventEmitter} from '../../../utils/EventEmitter.js';

// TODO: Extend with Redirect
export class MockCdpNetworkEvents {
  static defaultFrameId = '099A5216AF03AAFEC988F214B024DF08';
  static defaultUrl = 'http://www.google.com';

  cdpClient: CdpClient;

  url: string;
  requestId: string;
  fetchId: string;
  private loaderId: string;
  private frameId: string | undefined;
  private type: Protocol.Network.ResourceType;
  private initiator: Protocol.Network.Initiator;

  constructor(
    cdpClient: CdpClient,
    {
      requestId,
      fetchId,
      loaderId,
      url,
      frameId,
      type,
      initiator,
    }: Partial<{
      requestId: string;
      fetchId: string;
      loaderId: string;
      url: string;
      frameId: string | null;
      type: Protocol.Network.ResourceType;
      initiator: Protocol.Network.Initiator;
    }> = {},
  ) {
    this.cdpClient = cdpClient;

    this.requestId = requestId ?? '7151.2';
    this.fetchId = fetchId ?? 'interception-job-1.0';
    this.loaderId = loaderId ?? '7760711DEFCFA23132D98ABA6B4E175C';
    this.url = url ?? MockCdpNetworkEvents.defaultUrl;
    this.frameId =
      frameId === null ? undefined : MockCdpNetworkEvents.defaultFrameId;
    this.type = type ?? 'XHR';
    this.initiator = initiator ?? {
      type: 'script',
      stack: {
        callFrames: [
          {
            functionName: '',
            scriptId: '5',
            url: '',
            lineNumber: 0,
            columnNumber: 0,
          },
        ],
      },
    };
  }

  requestWillBeSent() {
    this.cdpClient.emit('Network.requestWillBeSent', {
      requestId: this.requestId,
      loaderId: this.loaderId,
      documentURL: this.url,
      request: {
        url: this.url,
        method: 'GET',
        headers: {
          'sec-ch-ua': '"Not-A.Brand";v="99", "Chromium";v="124"',
          Referer: 'http://localhost:49243/',
          'sec-ch-ua-mobile': '?0',
          'User-Agent':
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
          'sec-ch-ua-platform': '"macOS"',
        },
        mixedContentType: 'none',
        initialPriority: 'High',
        referrerPolicy: 'strict-origin-when-cross-origin',
        isSameSite: true,
      },
      timestamp: 2111.55635,
      wallTime: 1637315638.473634,
      initiator: this.initiator,
      redirectHasExtraInfo: false,
      type: this.type,
      frameId: this.frameId,
      hasUserGesture: false,
    });
  }

  requestWillBeSentRedirect() {
    this.cdpClient.emit('Network.requestWillBeSent', {
      requestId: this.requestId,
      loaderId: this.loaderId,
      documentURL: this.url,
      request: {
        url: this.url,
        method: 'GET',
        headers: {
          'Upgrade-Insecure-Requests': '1',
          'User-Agent':
            'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) HeadlessChrome/125.0.0.0 Safari/537.36',
          'sec-ch-ua': '"Chromium";v="125", "Not.A/Brand";v="24"',
          'sec-ch-ua-mobile': '?0',
          'sec-ch-ua-platform': '"Linux"',
        },
        mixedContentType: 'none',
        initialPriority: 'VeryHigh',
        referrerPolicy: 'strict-origin-when-cross-origin',
        isSameSite: true,
      },
      timestamp: 584528.247548,
      wallTime: 1712063242.363687,
      initiator: {type: 'other'},
      redirectHasExtraInfo: true,
      redirectResponse: {
        url: `${this.url}/redirect`,
        status: 302,
        statusText: 'Found',
        headers: {
          Connection: 'keep-alive',
          Date: 'Tue, 02 Apr 2024 13:07:22 GMT',
          'Transfer-Encoding': 'chunked',
          location: this.url,
        },
        mimeType: '',
        charset: '',
        connectionReused: true,
        connectionId: 41,
        remoteIPAddress: '[::1]',
        remotePort: 36387,
        fromDiskCache: false,
        fromServiceWorker: false,
        fromPrefetchCache: false,
        encodedDataLength: 134,
        timing: {
          requestTime: 584528.242766,
          proxyStart: -1,
          proxyEnd: -1,
          dnsStart: -1,
          dnsEnd: -1,
          connectStart: -1,
          connectEnd: -1,
          sslStart: -1,
          sslEnd: -1,
          workerStart: -1,
          workerReady: -1,
          workerFetchStart: -1,
          workerRespondWithSettled: -1,
          sendStart: 0.199,
          sendEnd: 0.258,
          pushStart: 0,
          pushEnd: 0,
          receiveHeadersStart: 4.199,
          receiveHeadersEnd: 4.304,
        },
        responseTime: 1.712063242363088e12,
        protocol: 'http/1.1',
        alternateProtocolUsage: 'unspecifiedReason',
        securityState: 'secure',
      },
      type: this.type,
      frameId: this.frameId,
      hasUserGesture: false,
    });
  }

  requestWillBeSentExtraInfo() {
    this.cdpClient.emit('Network.requestWillBeSentExtraInfo', {
      requestId: this.requestId,
      associatedCookies: [],
      headers: {
        Accept: '*/*',
        'Accept-Encoding': 'gzip, deflate, br, zstd',
        'Accept-Language': 'en-US,en;q=0.9',
        Connection: 'keep-alive',
        Host: 'localhost:49243',
        Referer: 'http://localhost:49243/',
        'Sec-Fetch-Dest': 'empty',
        'Sec-Fetch-Mode': 'cors',
        'Sec-Fetch-Site': 'same-origin',
        'User-Agent':
          'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
        'sec-ch-ua': '"Not-A.Brand";v="99", "Chromium";v="124"',
        'sec-ch-ua-mobile': '?0',
        'sec-ch-ua-platform': '"macOS"',
      },
      connectTiming: {requestTime: 2111.557593},
      clientSecurityState: {
        initiatorIsSecureContext: true,
        initiatorIPAddressSpace: 'Public',
        privateNetworkRequestPolicy: 'PermissionWarn',
      },
      siteHasCookieInOtherPartition: false,
    });
  }

  requestServedFromCache() {
    this.cdpClient.emit('Network.requestServedFromCache', {
      requestId: this.requestId,
    });
  }

  responseReceivedExtraInfo() {
    this.cdpClient.emit('Network.responseReceivedExtraInfo', {
      requestId: this.requestId,
      blockedCookies: [],
      headers: {
        'Cache-Control': 'no-cache, no-store',
        'Content-Type': 'text/html; charset=utf-8',
        Date: 'Fri, 19 Nov 2021 09:53:58 GMT',
        Connection: 'keep-alive',
        'Keep-Alive': 'timeout=5',
        'Content-Length': '0',
      },
      resourceIPAddressSpace: 'Public',
      statusCode: 200,
      headersText:
        'HTTP/1.1 200 OK\r\nCache-Control: no-cache, no-store\r\nContent-Type: text/html; charset=utf-8\r\nDate: Fri, 19 Nov 2021 09:53:58 GMT\r\nConnection: keep-alive\r\nKeep-Alive: timeout=5\r\nContent-Length: 0\r\n\r\n',
    });
  }

  responseReceivedExtraInfoRedirect() {
    this.cdpClient.emit('Network.responseReceivedExtraInfo', {
      requestId: this.requestId,
      blockedCookies: [],
      headers: {
        Connection: 'keep-alive',
        Date: 'Mon, 15 Apr 2024 11:53:20 GMT',
        'Transfer-Encoding': 'chunked',
        location: this.url,
      },
      resourceIPAddressSpace: 'Public',
      statusCode: 302,
      headersText:
        'HTTP/1.1 302 Found\r\nlocation: http://localhost:37363/empty.html\r\nDate: Mon, 15 Apr 2024 11:53:20 GMT\r\nConnection: keep-alive\r\nTransfer-Encoding: chunked\r\n\r\n',
      cookiePartitionKey: {
        topLevelSite: 'http://localhost',
        hasCrossSiteAncestor: false,
      },
      cookiePartitionKeyOpaque: false,
      exemptedCookies: [],
    });
  }

  responseReceived(hasExtraInfo = false) {
    this.cdpClient.emit('Network.responseReceived', {
      requestId: this.requestId,
      loaderId: this.loaderId,
      timestamp: 2111.563565,
      type: this.type,
      response: {
        url: this.url,
        status: 200,
        statusText: 'OK',
        headers: {
          'Cache-Control': 'no-cache, no-store',
          'Content-Type': 'text/html; charset=utf-8',
          Date: 'Fri, 19 Nov 2021 09:53:58 GMT',
          Connection: 'keep-alive',
          'Keep-Alive': 'timeout=5',
          'Content-Length': '0',
        },
        // TODO: set to a correct one
        charset: '',
        mimeType: 'text/html',
        connectionReused: true,
        connectionId: 322,
        remoteIPAddress: '[::1]',
        remotePort: 8907,
        fromDiskCache: false,
        fromServiceWorker: false,
        fromPrefetchCache: false,
        encodedDataLength: 197,
        timing: {
          receiveHeadersStart: 0,
          requestTime: 2111.561759,
          proxyStart: -1,
          proxyEnd: -1,
          dnsStart: -1,
          dnsEnd: -1,
          connectStart: -1,
          connectEnd: -1,
          sslStart: -1,
          sslEnd: -1,
          workerStart: -1,
          workerReady: -1,
          workerFetchStart: -1,
          workerRespondWithSettled: -1,
          sendStart: 0.148,
          sendEnd: 0.19,
          pushStart: 0,
          pushEnd: 0,
          receiveHeadersEnd: 0.925,
        },
        responseTime: 1.637315638479928e12,
        protocol: 'http/1.1',
        securityState: 'secure',
      },
      hasExtraInfo,
      frameId: this.frameId,
    });
  }

  requestPaused() {
    this.cdpClient.emit('Fetch.requestPaused', {
      requestId: this.fetchId,
      request: {
        url: this.url,
        method: 'GET',
        headers: {
          Accept: '*/*',
          Referer: 'http://localhost:49243/',
          'User-Agent':
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
          'sec-ch-ua': '"Not-A.Brand";v="99", "Chromium";v="124"',
          'sec-ch-ua-mobile': '?0',
          'sec-ch-ua-platform': '"macOS"',
        },
        initialPriority: 'High',
        referrerPolicy: 'strict-origin-when-cross-origin',
      },
      // FrameId is required in fetch.
      frameId: this.frameId ?? MockCdpNetworkEvents.defaultFrameId,
      resourceType: this.type,
      networkId: this.requestId,
    });
  }

  responsePaused() {
    this.cdpClient.emit('Fetch.requestPaused', {
      requestId: this.fetchId,
      request: {
        url: this.url,
        method: 'GET',
        headers: {},
        initialPriority: 'VeryHigh',
        referrerPolicy: 'strict-origin-when-cross-origin',
      },
      // TODO: fill for response correctly
      responseStatusCode: 200,
      responseStatusText: '',
      responseHeaders: [],
      // FrameId is required in fetch.
      frameId: this.frameId ?? MockCdpNetworkEvents.defaultFrameId,
      resourceType: this.type,
      networkId: this.requestId,
    });
  }

  authRequired() {
    this.cdpClient.emit('Fetch.authRequired', {
      requestId: this.fetchId,
      request: {
        url: this.url,
        method: 'GET',
        headers: {
          Accept: '*/*',
          Referer: 'http://localhost:49243/',
          'User-Agent':
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36',
          'sec-ch-ua': '"Not-A.Brand";v="99", "Chromium";v="124"',
          'sec-ch-ua-mobile': '?0',
          'sec-ch-ua-platform': '"macOS"',
        },
        initialPriority: 'High',
        referrerPolicy: 'strict-origin-when-cross-origin',
      },
      // FrameId is required in fetch.
      frameId: this.frameId ?? MockCdpNetworkEvents.defaultFrameId,
      resourceType: this.type,
      authChallenge: {
        source: 'Server',
        origin: new URL(this.url).origin,
        scheme: 'basic',
        realm: 'Access to staging site',
      },
    });
  }

  loadingFailed() {
    this.cdpClient.emit('Network.loadingFailed', {
      requestId: this.requestId,
      timestamp: 279179.745291,
      type: 'Fetch',
      errorText: 'net::ERR_NAME_NOT_RESOLVED',
      canceled: false,
    });
  }

  loadingFinished() {
    this.cdpClient.emit('Network.loadingFinished', {
      requestId: this.requestId,
      timestamp: 279179.745291,
      encodedDataLength: 999,
    });
  }

  setJsonEvent(json: string | Record<string, unknown>, _normalize = false) {
    const event = json instanceof Object ? json : JSON.parse(json);

    const replaceKeys = [
      ['requestId', this.requestId],
      ['networkId', this.requestId],
      ['loaderId', this.loaderId],
      ['frameId', this.frameId],
      ['url', this.url],
    ] as const;
    for (const [key, value] of replaceKeys) {
      this.findAndReplaceKey(event, key, value);
    }

    this.cdpClient.emit(event.method, event.params);
  }

  findAndReplaceKey(
    source: Record<string, unknown>,
    searchKey: string,
    value: unknown,
  ): void {
    for (const key of Object.keys(source)) {
      if (key === searchKey) {
        source[key] = value;
        return;
      }
      if (typeof source[key] === 'object' && !Array.isArray(source[key])) {
        this.findAndReplaceKey(
          source[key] as Record<string, unknown>,
          searchKey,
          value,
        );
      }
    }
  }
}

export class MockCdpClient extends EventEmitter<CdpEvents> {
  #logger: (...args: any[]) => void;

  sessionId = '23E8E97ED43449740710991CD32AEFA3';

  constructor(logger: (...args: any[]) => void) {
    super();
    this.#logger = logger;
  }

  sendCommand(...args: any[]) {
    this.#logger(...args);

    return new Promise((resolve) => {
      setTimeout(resolve, 100);
    });
  }

  isCloseError(error: unknown) {
    return error instanceof CloseError;
  }
}

export class MockCdpTarget {
  cdpClient: CdpClient;
  #logger: (...args: any[]) => void;

  constructor(logger: (...args: any[]) => void) {
    this.#logger = logger;
    this.cdpClient = new MockCdpClient(logger) as CdpClient;
  }

  enableFetchIfNeeded() {
    this.#logger('Fetch.enabled called');
    return Promise.resolve();
  }

  get topLevelId() {
    return MockCdpNetworkEvents.defaultFrameId;
  }

  isSubscribedTo() {
    return true;
  }
}
