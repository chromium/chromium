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
import {expect} from 'chai';
import type {Protocol} from 'devtools-protocol';

import type {CdpClient, CdpEvents} from '../../../cdp/CdpClient.js';
import {ChromiumBidi, Network} from '../../../protocol/protocol.js';
import {EventEmitter} from '../../../utils/EventEmitter.js';
import {ProcessingQueue} from '../../../utils/ProcessingQueue.js';
import type {OutgoingMessage} from '../../OutgoingMessage.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import type {CdpTarget} from '../context/CdpTarget.js';
import {EventManager, EventManagerEvents} from '../session/EventManager.js';

import {NetworkStorage} from './NetworkStorage.js';

// type Ka = keyof Protocol.Network.RequestWillBeSentEvent;
// type Kb = keyof Protocol.Network.RequestWillBeSentExtraInfoEvent;
// type Kc = keyof Protocol.Network.ResponseReceivedExtraInfoEvent;
// type Kd = keyof Protocol.Network.ResponseReceivedEvent;
// type Kz = Exclude<(Ka & Kb) | (Ka & Kd) | (Kb & Kc), 'headers' | 'timestamp'>;

// TODO: Extend with Redirect
class MockCdpNetworkEvents {
  static defaultFrameId = '099A5216AF03AAFEC988F214B024DF08';
  static defaultUrl = 'http://www.google.com';

  cdpClient: CdpClient;

  requestId: string;
  fetchId: string;
  private loaderId: string;
  private url: string;
  private frameId: string;
  private type: Protocol.Network.ResourceType;

  constructor(
    cdpClient: CdpClient,
    {
      requestId,
      fetchId,
      loaderId,
      url,
      frameId,
      type,
    }: Partial<{
      requestId: string;
      fetchId: string;
      loaderId: string;
      url: string;
      frameId: string;
      type: Protocol.Network.ResourceType;
    }> = {}
  ) {
    this.cdpClient = cdpClient;

    this.requestId = requestId ?? '7760711DEFCFA23132D98ABA6B4E175C';
    this.fetchId = fetchId ?? 'interception-job-1.0';
    this.loaderId = loaderId ?? '7760711DEFCFA23132D98ABA6B4E175C';
    this.url = url ?? MockCdpNetworkEvents.defaultUrl;
    this.frameId = frameId ?? MockCdpNetworkEvents.defaultFrameId;
    this.type = type ?? 'Document';
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
          'Upgrade-Insecure-Requests': '1',
          'User-Agent':
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) HeadlessChrome/97.0.4691.0 Safari/537.36',
        },
        mixedContentType: 'none',
        initialPriority: 'VeryHigh',
        referrerPolicy: 'strict-origin-when-cross-origin',
        isSameSite: true,
      },
      timestamp: 2111.55635,
      wallTime: 1637315638.473634,
      initiator: {type: 'other'},
      redirectHasExtraInfo: false,
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
        Host: 'localhost:8907',
        Connection: 'keep-alive',
        'Upgrade-Insecure-Requests': '1',
        'User-Agent':
          'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) HeadlessChrome/97.0.4691.0 Safari/537.36',
        Accept:
          'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9',
        'Sec-Fetch-Site': 'none',
        'Sec-Fetch-Mode': 'navigate',
        'Sec-Fetch-User': '?1',
        'Sec-Fetch-Dest': 'document',
        'Accept-Encoding': 'gzip, deflate, br',
      },
      connectTiming: {requestTime: 2111.557593},
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
      resourceIPAddressSpace: 'Local',
      statusCode: 200,
      headersText:
        'HTTP/1.1 200 OK\r\nCache-Control: no-cache, no-store\r\nContent-Type: text/html; charset=utf-8\r\nDate: Fri, 19 Nov 2021 09:53:58 GMT\r\nConnection: keep-alive\r\nKeep-Alive: timeout=5\r\nContent-Length: 0\r\n\r\n',
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
        headers: {},
        initialPriority: 'VeryHigh',
        referrerPolicy: 'strict-origin-when-cross-origin',
      },
      frameId: this.frameId,
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
      frameId: this.frameId,
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
        headers: {},
        initialPriority: 'VeryHigh',
        referrerPolicy: 'strict-origin-when-cross-origin',
      },
      frameId: this.frameId,
      resourceType: this.type,
      // TODO: fill for auth challenge correctly
      authChallenge: {
        source: 'Server',
        origin: new URL(this.url).origin,
        scheme: 'Basic',
        realm: 'Access to staging site',
      },
    });
  }
}

class MockCdpClient extends EventEmitter<CdpEvents> {
  sessionId = '23E8E97ED43449740710991CD32AEFA3';
  sendCommand() {
    return Promise.resolve();
  }

  isCloseError() {
    return false;
  }
}

class MockCdpTarget {
  cdpClient = new MockCdpClient() as CdpClient;
  enableFetchIfNeeded() {
    return Promise.resolve();
  }
}

describe.only('NetworkStorage', () => {
  let processedEvents = new Map<
    ChromiumBidi.Event['method'],
    ChromiumBidi.Event['params']
  >();
  let networkStorage!: NetworkStorage;
  let cdpClient!: CdpClient;

  // TODO: Better way of getting Events
  async function getEvent(name: ChromiumBidi.Event['method']) {
    await new Promise((resolve) => setTimeout(resolve, 0));

    return processedEvents.get(name);
  }
  beforeEach(() => {
    processedEvents = new Map();
    const browsingContextStorage = new BrowsingContextStorage();
    const cdpTarget = new MockCdpTarget() as unknown as CdpTarget;
    const browsingContext = {
      cdpTarget,
      id: MockCdpNetworkEvents.defaultFrameId,
    } as unknown as BrowsingContextImpl;
    cdpClient = cdpTarget.cdpClient;
    // We need to add it the storage to emit properly
    browsingContextStorage.addContext(browsingContext);
    const eventManager = new EventManager(browsingContextStorage);
    const processingQueue = new ProcessingQueue<OutgoingMessage>(
      async ({message}) => {
        const cdpEvent = message as unknown as ChromiumBidi.Event;
        processedEvents.set(cdpEvent.method, cdpEvent.params);
        return await Promise.resolve();
      }
    );
    // Subscribe to the `network` module globally
    eventManager.subscriptionManager.subscribe(
      ChromiumBidi.BiDiModule.Network,
      null,
      null
    );
    eventManager.on(EventManagerEvents.Event, ({message, event}) => {
      processingQueue.add(message, event);
    });
    networkStorage = new NetworkStorage(eventManager, {
      on(): void {
        // Used for clearing on target disconnect
      },
    } as unknown as CdpClient);

    networkStorage.onCdpTargetCreated(cdpTarget);
  });

  describe('network.beforeRequestSent', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.requestWillBeSentExtraInfo();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.exist;
    });

    it('should work for reverse order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSentExtraInfo();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.requestWillBeSent();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.exist;
    });

    it('should work interception', async () => {
      const interception = await networkStorage.addIntercept({
        urlPatterns: [
          {type: 'string', pattern: MockCdpNetworkEvents.defaultUrl},
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.requestPaused();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.deep.include({
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should work interception pause first', async () => {
      const interception = await networkStorage.addIntercept({
        urlPatterns: [
          {type: 'string', pattern: MockCdpNetworkEvents.defaultUrl},
        ],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.requestWillBeSent();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.deep.include({
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should work non blocking interception', async () => {
      await networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: 'http://not.correct.com'}],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.requestWillBeSentExtraInfo();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.deep.include({
        isBlocked: false,
      });
    });
  });

  describe('network.responseStarted', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived();
      const event = await getEvent('network.responseStarted');
      expect(event).to.exist;
    });
    it('should work for normal order no extraInfo', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived(false);
      const event = await getEvent('network.responseStarted');
      expect(event).to.exist;
    });
    it('should work for reverse order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      request.responseReceivedExtraInfo();
      let event = await getEvent('network.responseStarted');
      expect(event).to.not.exist;
      request.responseReceived();

      event = await getEvent('network.responseStarted');
      expect(event).to.exist;
    });
    it('should work interception', async () => {
      const interception = await networkStorage.addIntercept({
        urlPatterns: [
          {type: 'string', pattern: MockCdpNetworkEvents.defaultUrl},
        ],
        phases: [Network.InterceptPhase.ResponseStarted],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      let event = await getEvent('network.responseStarted');
      expect(event).to.not.exist;
      request.responsePaused();

      event = await getEvent('network.responseStarted');

      expect(event).to.deep.include({
        isBlocked: true,
        intercepts: [interception],
      });
    });
    it('should work non blocking interception', async () => {
      await networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: 'http://not.correct.com'}],
        phases: [Network.InterceptPhase.ResponseStarted],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      let event = await getEvent('network.responseStarted');
      expect(event).to.not.exist;

      request.responsePaused();
      event = await getEvent('network.responseStarted');
      expect(event).to.not.exist;

      request.responseReceived();
      request.responseReceivedExtraInfo();

      event = await getEvent('network.responseStarted');
      expect(event).to.deep.include({
        isBlocked: false,
      });
    });
  });

  describe('network.authRequired', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestPaused();
      request.requestWillBeSentExtraInfo();
      request.authRequired();
      const event = await getEvent('network.authRequired');
      expect(event).to.exist;
      request.requestWillBeSent();
    });
  });
});
