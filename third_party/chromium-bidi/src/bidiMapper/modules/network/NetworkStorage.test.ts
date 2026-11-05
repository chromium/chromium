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
import {describe, it, beforeEach} from 'node:test';
import {assert} from 'chai';
import sinon from 'sinon';

import type {Protocol} from 'devtools-protocol';

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {
  ChromiumBidi,
  Network,
  NoSuchNetworkCollectorException,
  NoSuchNetworkDataException,
} from '../../../protocol/protocol.js';
import {ProcessingQueue} from '../../../utils/ProcessingQueue.js';
import type {OutgoingMessage} from '../../OutgoingMessage.js';
import {UserContextStorage} from '../browser/UserContextStorage.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {EventManager, EventManagerEvents} from '../session/EventManager.js';

import {
  MockCdpNetworkEvents,
  MockCdpTarget,
} from './NetworkModuleMocks.test.js';
import {NetworkProcessor} from './NetworkProcessor.js';
import {NetworkStorage} from './NetworkStorage.js';

function logger(...args: any[]) {
  // eslint-disable-next-line no-constant-condition
  if (false) {
    return () => {
      // eslint-disable-next-line no-console
      console.log(...args);
    };
  }
  return;
}

describe('NetworkStorage', () => {
  let processedEvents: [
    ChromiumBidi.Event['method'],
    ChromiumBidi.Event['params'],
  ][] = [];
  let eventManager!: EventManager;
  let browsingContextStorage!: BrowsingContextStorage;
  let networkStorage!: NetworkStorage;
  let cdpClient!: CdpClient;
  let processingQueue!: ProcessingQueue<OutgoingMessage>;

  // TODO: Better way of getting Events
  async function getEvent(name: ChromiumBidi.Event['method']) {
    await new Promise((resolve) => setTimeout(resolve, 0));

    return processedEvents
      .reverse()
      .find(([method]) => method === name)
      ?.at(1);
  }
  async function getEvents(name: ChromiumBidi.Event['method']) {
    await new Promise((resolve) => setTimeout(resolve, 0));

    return processedEvents.filter(([method]) => method === name);
  }

  beforeEach(() => {
    processedEvents = [];
    browsingContextStorage = new BrowsingContextStorage();
    const cdpTarget = new MockCdpTarget(logger) as unknown as CdpTarget;
    const browsingContext = {
      cdpTarget,
      id: MockCdpNetworkEvents.defaultFrameId,
    } as unknown as BrowsingContextImpl;
    cdpClient = cdpTarget.cdpClient;
    const userContextStorage = new UserContextStorage(cdpClient);
    // We need to add it the storage to emit properly
    browsingContextStorage.addContext(browsingContext);
    eventManager = new EventManager(browsingContextStorage, userContextStorage);
    processingQueue = new ProcessingQueue<OutgoingMessage>(
      async ({message}) => {
        if (message.type === 'event') {
          processedEvents.push([message.method, message.params]);
        }
        return await Promise.resolve();
      },
      logger,
    );
    // Subscribe to the `network` module globally
    eventManager.subscriptionManager.subscribe(
      [ChromiumBidi.BiDiModule.Network],
      // Verify that the Request send the message
      // To the correct context
      [MockCdpNetworkEvents.defaultFrameId],
      [],
      null,
    );
    eventManager.on(EventManagerEvents.Event, ({message, event}) => {
      processingQueue.add(message, event);
    });
    networkStorage = new NetworkStorage(eventManager, browsingContextStorage, {
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
      assert.notExists(event);

      request.requestWillBeSentExtraInfo();
      event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
    });

    it('should work for reverse order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSentExtraInfo();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.requestWillBeSent();
      event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
    });

    it('should work interception', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestWillBeSent();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.requestPaused();
      event = await getEvent('network.beforeRequestSent');
      assert.deepInclude(event, {
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should work interception pause first', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.requestWillBeSent();
      event = await getEvent('network.beforeRequestSent');
      assert.deepInclude(event, {
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should work non blocking interception', async () => {
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: 'http://not.correct.com'},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.requestWillBeSentExtraInfo();
      event = await getEvent('network.beforeRequestSent');
      assert.deepInclude(event, {
        isBlocked: false,
      });
    });

    it('should prefer browser Fetch.requestPaused metadata over renderer Network.requestWillBeSent', async () => {
      const victimUrl = 'https://api.victim.example/v1/me';
      const attackerUrl = 'http://evil.example/collect';
      const attackerFrameId = 'ATTACKER_FRAME_ID';
      browsingContextStorage.addContext({
        id: attackerFrameId,
      } as unknown as BrowsingContextImpl);
      eventManager.subscriptionManager.subscribe(
        [ChromiumBidi.BiDiModule.Network],
        [attackerFrameId],
        [],
        null,
      );
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: victimUrl},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      const request = new MockCdpNetworkEvents(cdpClient, {
        url: victimUrl,
      });
      // Renderer emits forged URL, method, frameId and headers in requestWillBeSent.
      cdpClient.emit('Network.requestWillBeSent', {
        requestId: request.requestId,
        loaderId: 'LOADER_ID',
        documentURL: victimUrl,
        request: {
          url: victimUrl,
          urlFragment: '#forged',
          method: 'POST',
          headers: {
            'X-Forged-Header': 'forged',
          },
          mixedContentType: 'none',
          initialPriority: 'High',
          referrerPolicy: 'strict-origin-when-cross-origin',
          isSameSite: true,
        },
        timestamp: 2111.55635,
        wallTime: 1637315638.473634,
        initiator: {type: 'script'},
        redirectHasExtraInfo: false,
        type: 'Fetch',
        frameId: MockCdpNetworkEvents.defaultFrameId,
        hasUserGesture: false,
      });

      // Browser emits authoritative URL, method, frameId and headers in requestPaused.
      cdpClient.emit('Fetch.requestPaused', {
        requestId: request.fetchId,
        networkId: request.requestId,
        frameId: attackerFrameId,
        resourceType: 'Fetch',
        request: {
          url: attackerUrl,
          method: 'GET',
          headers: {
            'X-Real-Header': 'real',
          },
          initialPriority: 'High',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
      });

      // Because the real URL does not match the victim intercept, the request
      // must not be blocked and waits for extraInfo before emitting.
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.requestWillBeSentExtraInfo();
      event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
      assert.deepInclude(event, {
        context: attackerFrameId,
        isBlocked: false,
      });
      const beforeRequestSent = event as Network.BeforeRequestSentParameters;
      assert.strictEqual(beforeRequestSent.request.url, attackerUrl);
      assert.strictEqual(beforeRequestSent.request.method, 'GET');
      assert.isUndefined(
        beforeRequestSent.request.headers.find(
          (h) => h.name === 'X-Forged-Header',
        ),
      );
      assert.deepInclude(beforeRequestSent.request.headers, {
        name: 'X-Real-Header',
        value: {type: 'string', value: 'real'},
      });
    });

    it('should block when browser Fetch.requestPaused URL matches intercept despite forged Network.requestWillBeSent URL', async () => {
      const forgedUrl = 'http://innocent.example/ok';
      const realUrl = 'http://evil.example/collect';
      const interception = networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: realUrl},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      const request = new MockCdpNetworkEvents(cdpClient, {
        url: forgedUrl,
      });
      request.requestWillBeSent();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      // Browser reports the real URL matching the intercept.
      cdpClient.emit('Fetch.requestPaused', {
        requestId: request.fetchId,
        networkId: request.requestId,
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'XHR',
        request: {
          url: realUrl,
          urlFragment: '#real',
          method: 'POST',
          headers: {},
          initialPriority: 'High',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
      });

      event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
      assert.deepInclude(event, {
        isBlocked: true,
        intercepts: [interception],
      });
      const beforeRequestSent = event as Network.BeforeRequestSentParameters;
      assert.strictEqual(beforeRequestSent.request.url, `${realUrl}#real`);
      assert.strictEqual(beforeRequestSent.request.method, 'POST');
    });

    it('should work with non blocking interception and fail response', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: 'http://not.correct.com'},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestWillBeSent();
      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      assert.notExists(event);

      request.loadingFailed();
      event = await getEvent('network.beforeRequestSent');
      assert.deepInclude(event, {
        isBlocked: false,
      });
    });

    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      const event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
    });

    it('should work with data url and global interception', async () => {
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([{type: 'pattern'}]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      const event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
    });

    it('should report right context for preflight requests', async () => {
      const initialRequest = new MockCdpNetworkEvents(cdpClient);
      initialRequest.requestWillBeSent();

      const preflightRrequest = new MockCdpNetworkEvents(cdpClient, {
        requestId: 'ANOTHER_REQUEST_ID',
        // Preflight requests don't have `frameId`.
        frameId: null,
        initiator: {
          type: 'preflight',
          url: initialRequest.url,
          requestId: initialRequest.requestId,
        },
      });
      preflightRrequest.requestWillBeSent();
      preflightRrequest.requestWillBeSentExtraInfo();

      const event = await getEvent('network.beforeRequestSent');
      assert.exists(event);
      assert.equal(
        (event as Network.BeforeRequestSentParameters).context,
        MockCdpNetworkEvents.defaultFrameId,
      );
    });
  });

  describe('network.responseStarted', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived();
      const event = await getEvent('network.responseStarted');
      assert.exists(event);
    });

    it('should work for normal order no extraInfo', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived(false);
      const event = await getEvent('network.responseStarted');
      assert.exists(event);
    });

    it('should work for reverse order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      request.responseReceivedExtraInfo();
      let event = await getEvent('network.responseStarted');
      assert.notExists(event);
      request.responseReceived();

      event = await getEvent('network.responseStarted');
      assert.exists(event);
    });

    it('should work interception', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.ResponseStarted],
      });

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      let event = await getEvent('network.responseStarted');
      assert.notExists(event);
      request.responsePaused();

      event = await getEvent('network.responseStarted');

      assert.deepInclude(event, {
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should emit beforeRequestSent before a blocked responseStarted without request ExtraInfo', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.ResponseStarted],
      });

      request.requestWillBeSent();
      // The response is paused before `Network.requestWillBeSentExtraInfo`.
      request.responsePaused();

      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.deepEqual(
        processedEvents.map(([method]) => method),
        ['network.beforeRequestSent', 'network.responseStarted'],
      );
    });

    it('should work non blocking interception', async () => {
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: 'http://not.correct.com'},
        ]),
        phases: [Network.InterceptPhase.ResponseStarted],
      });
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();

      let event = await getEvent('network.responseStarted');
      assert.notExists(event);

      request.responsePaused();
      event = await getEvent('network.responseStarted');
      assert.notExists(event);

      request.responseReceived();
      request.responseReceivedExtraInfo();

      event = await getEvent('network.responseStarted');
      assert.deepInclude(event, {
        isBlocked: false,
      });
    });

    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.responseReceived();
      const event = await getEvent('network.responseStarted');
      assert.exists(event);
    });
  });

  describe('network.authRequired', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestPaused();
      request.requestWillBeSentExtraInfo();
      request.authRequired();
      const event = await getEvent('network.authRequired');
      assert.deepNestedInclude(event, {
        'request.request': request.requestId,
        'request.method': 'GET',
      });
    });

    it('should work with only authRequired', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.authRequired();
      const event = await getEvent('network.authRequired');
      assert.deepNestedInclude(event, {
        'request.request': request.fetchId,
        'request.method': 'GET',
      });
    });

    it('should work report multiple authRequired', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.authRequired();
      let events = await getEvents('network.authRequired');
      assert.lengthOf(events, 1);
      request.authRequired();
      events = await getEvents('network.authRequired');
      assert.lengthOf(events, 2);
    });

    it('should emit beforeRequestSent before authRequired', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.AuthRequired],
      });

      request.requestWillBeSent();
      request.requestPaused();
      request.authRequired();

      const event = await getEvent('network.beforeRequestSent');
      assert.exists(event);

      const authEvent = await getEvent('network.authRequired');
      assert.exists(authEvent);

      const beforeRequestSentIndex = processedEvents.findIndex(
        ([method]) => method === 'network.beforeRequestSent',
      );
      const authRequiredIndex = processedEvents.findIndex(
        ([method]) => method === 'network.authRequired',
      );

      assert.isBelow(beforeRequestSentIndex, authRequiredIndex);
    });
  });

  describe('network.responseCompleted', () => {
    for (const {name, encodedDataLength, encodedBodyLength} of [
      {
        name: 'uncompressed body',
        encodedDataLength: 2151,
        encodedBodyLength: 2000,
      },
      {
        name: 'empty body',
        encodedDataLength: 151,
        encodedBodyLength: 0,
      },
      {
        name: 'cached body',
        encodedDataLength: 0,
        encodedBodyLength: 2000,
      },
    ]) {
      it(`should distinguish encoded body size from transferred bytes for ${name}`, async () => {
        const request = new MockCdpNetworkEvents(cdpClient);

        request.requestWillBeSent();
        request.requestWillBeSentExtraInfo();
        if (encodedDataLength === 0) {
          request.requestServedFromCache();
        }
        request.responseReceived(false);
        request.setJsonEvent({
          method: 'Network.loadingFinished',
          params: {
            requestId: request.requestId,
            timestamp: 279179.745291,
            encodedDataLength,
            encodedBodyLength,
          },
        });

        const event = await getEvent('network.responseCompleted');
        assert.deepNestedInclude(event, {
          'response.bodySize': encodedBodyLength,
          'response.bytesReceived': encodedDataLength,
        });
      });
    }

    it('should report an unknown body size when CDP does not provide it', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived(false);
      request.loadingFinished();

      const event = await getEvent('network.responseCompleted');
      assert.deepNestedInclude(event, {
        'response.bodySize': null,
        'response.bytesReceived': 999,
      });
    });

    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.responseReceived();
      request.loadingFinished();
      const event = await getEvent('network.responseCompleted');
      assert.exists(event);
    });

    it('should work with redirect', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.requestWillBeSentRedirect();
      const event = await getEvent('network.responseCompleted');
      assert.exists(event);
    });

    it('should emit response events if an intercept is added after beforeRequestSent', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      assert.lengthOf(await getEvents('network.beforeRequestSent'), 1);

      // The client enables interception while the request is in flight.
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      request.responseReceived();
      request.loadingFinished();

      await new Promise((resolve) => setTimeout(resolve, 0));
      assert.deepEqual(
        processedEvents.map(([method]) => method),
        [
          'network.beforeRequestSent',
          'network.responseStarted',
          'network.responseCompleted',
        ],
      );
      assert.isUndefined(networkStorage.getRequestById(request.requestId));
    });
  });

  describe('redirects', () => {
    it('should return 200 from last response', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestWillBeSentRedirect();
      request.responseReceivedExtraInfoRedirect();
      request.requestWillBeSentExtraInfo();
      request.responseReceived();
      request.responseReceivedExtraInfo();
      request.loadingFinished();

      const event = await getEvent('network.responseCompleted');

      assert.deepNestedInclude(event, {
        'response.status': 200,
      });
    });
  });

  describe('redirects with late ExtraInfo', () => {
    const REQUEST_ID = 'REDIRECT_REQUEST_ID';
    const LOADER_ID = 'REDIRECT_LOADER_ID';
    const FETCH_ID = 'REDIRECT_FETCH_ID';
    const URL_A = 'http://localhost/foo.html';
    const URL_B = 'http://localhost/title.html';
    const RELATIVE_LOCATION = '/title.html';
    const URL_C = 'http://localhost/c.html';
    const RELATIVE_LOCATION_C = '/c.html';
    const URL_HOME = 'http://localhost/home';
    const RELATIVE_LOCATION_HOME = '/home';
    const HOP_HEADER = 'X-Hop';

    function cdpResponse(
      url: string,
      status: number,
      headers: Protocol.Network.Headers,
    ): Protocol.Network.Response {
      return {
        url,
        status,
        statusText: status === 302 ? 'Found' : 'OK',
        headers,
        mimeType: 'text/html',
        charset: '',
        connectionReused: true,
        connectionId: 1,
        encodedDataLength: 100,
        securityState: 'secure',
      };
    }

    function requestWillBeSent(
      url: string,
      redirect?: {from: string; hasExtraInfo: boolean; location?: string},
    ) {
      cdpClient.emit('Network.requestWillBeSent', {
        requestId: REQUEST_ID,
        loaderId: LOADER_ID,
        documentURL: url,
        request: {
          url,
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
        timestamp: 1,
        wallTime: 1,
        initiator: {type: 'other'},
        redirectHasExtraInfo: redirect?.hasExtraInfo ?? false,
        ...(redirect
          ? {
              redirectResponse: cdpResponse(redirect.from, 302, {
                location: redirect.location ?? RELATIVE_LOCATION,
              }),
            }
          : {}),
        type: 'Document',
        frameId: MockCdpNetworkEvents.defaultFrameId,
      });
    }

    function requestWillBeSentExtraInfo(hop: string, cookieValue?: string) {
      cdpClient.emit('Network.requestWillBeSentExtraInfo', {
        requestId: REQUEST_ID,
        associatedCookies: cookieValue
          ? [
              {
                blockedReasons: [],
                cookie: {
                  name: 'username',
                  value: cookieValue,
                  domain: 'localhost',
                  path: '/',
                  expires: -1,
                  size: 8 + cookieValue.length,
                  httpOnly: false,
                  secure: false,
                  session: true,
                  priority: 'Medium',
                  sourceScheme: 'NonSecure',
                  sourcePort: 80,
                },
              },
            ]
          : [],
        headers: {
          [HOP_HEADER]: hop,
          ...(cookieValue ? {Cookie: `username=${cookieValue}`} : {}),
        },
        connectTiming: {requestTime: 1},
      });
    }

    function responseReceivedExtraInfo(
      statusCode: number,
      headers: Protocol.Network.Headers = {},
    ) {
      cdpClient.emit('Network.responseReceivedExtraInfo', {
        requestId: REQUEST_ID,
        blockedCookies: [],
        headers,
        resourceIPAddressSpace: 'Local',
        statusCode,
      });
    }

    function responseReceived(hasExtraInfo: boolean, url = URL_B) {
      cdpClient.emit('Network.responseReceived', {
        requestId: REQUEST_ID,
        loaderId: LOADER_ID,
        timestamp: 2,
        type: 'Document',
        response: cdpResponse(url, 200, {}),
        hasExtraInfo,
        frameId: MockCdpNetworkEvents.defaultFrameId,
      });
    }

    function loadingFinished() {
      cdpClient.emit('Network.loadingFinished', {
        requestId: REQUEST_ID,
        timestamp: 3,
        encodedDataLength: 100,
      });
    }

    function loadingFailed() {
      cdpClient.emit('Network.loadingFailed', {
        requestId: REQUEST_ID,
        timestamp: 3,
        type: 'Document',
        errorText: 'net::ERR_CONNECTION_RESET',
        canceled: false,
      });
    }

    function summarize(
      events: Awaited<ReturnType<typeof getHopEvents>>,
    ): unknown[][] {
      return events.map(({method, redirectCount, hop, status}) => [
        method,
        redirectCount,
        hop,
        status,
      ]);
    }

    function getHeader(
      headers: Network.Header[],
      name: string,
    ): string | undefined {
      return headers.find(
        (header) => header.name.toLowerCase() === name.toLowerCase(),
      )?.value.value;
    }

    /** Returns a summary of the emitted events in the emitted order. */
    async function getHopEvents() {
      await new Promise((resolve) => setTimeout(resolve, 0));
      return processedEvents.map(([method, params]) => {
        const eventParams = params as Network.BaseParameters & {
          response?: Network.ResponseData;
        };
        return {
          method,
          redirectCount: eventParams.redirectCount,
          url: eventParams.request.url,
          hop: getHeader(eventParams.request.headers, HOP_HEADER),
          status: eventParams.response?.status,
        };
      });
    }

    function getParams<T>(method: string, redirectCount: number): T {
      const params = processedEvents.find(
        ([eventMethod, eventParams]) =>
          eventMethod === method &&
          (eventParams as Network.BaseParameters).redirectCount ===
            redirectCount,
      )?.[1];
      assert.exists(params);
      return params as T;
    }

    it('should apply late request ExtraInfo to the redirected hop', async () => {
      // The event order from a failing Puppeteer CI run, with an early
      // response of the next hop.
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      responseReceived(true);

      assert.deepEqual(await getHopEvents(), []);

      requestWillBeSentExtraInfo('A', 'John Doe');
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION});
      requestWillBeSentExtraInfo('B', 'John Doe');
      responseReceivedExtraInfo(200);
      loadingFinished();

      assert.deepEqual(await getHopEvents(), [
        {
          method: 'network.beforeRequestSent',
          redirectCount: 0,
          url: URL_A,
          hop: 'A',
          status: undefined,
        },
        {
          method: 'network.responseStarted',
          redirectCount: 0,
          url: URL_A,
          hop: 'A',
          status: 302,
        },
        {
          method: 'network.responseCompleted',
          redirectCount: 0,
          url: URL_A,
          hop: 'A',
          status: 302,
        },
        {
          method: 'network.beforeRequestSent',
          redirectCount: 1,
          url: URL_B,
          hop: 'B',
          status: undefined,
        },
        {
          method: 'network.responseStarted',
          redirectCount: 1,
          url: URL_B,
          hop: 'B',
          status: 200,
        },
        {
          method: 'network.responseCompleted',
          redirectCount: 1,
          url: URL_B,
          hop: 'B',
          status: 200,
        },
      ]);

      for (const redirectCount of [0, 1]) {
        const event = getParams<Network.BeforeRequestSentParameters>(
          'network.beforeRequestSent',
          redirectCount,
        );
        assert.strictEqual(
          getHeader(event.request.headers, 'cookie'),
          'username=John Doe',
        );
        assert.lengthOf(event.request.cookies, 1);
        assert.strictEqual(event.request.cookies[0]!.name, 'username');
      }
    });

    it('should not apply late response ExtraInfo of the redirected hop to the next hop', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      requestWillBeSentExtraInfo('B');
      responseReceived(true);
      // The 302 response ExtraInfo of the first hop arrives late.
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION});
      loadingFinished();
      responseReceivedExtraInfo(200);

      const events = await getHopEvents();
      assert.deepEqual(
        events.map(({method, redirectCount, hop, status}) => [
          method,
          redirectCount,
          hop,
          status,
        ]),
        [
          ['network.beforeRequestSent', 0, 'A', undefined],
          ['network.responseStarted', 0, 'A', 302],
          ['network.responseCompleted', 0, 'A', 302],
          ['network.beforeRequestSent', 1, 'B', undefined],
          ['network.responseStarted', 1, 'B', 200],
          ['network.responseCompleted', 1, 'B', 200],
        ],
      );
      const completedA = getParams<Network.ResponseCompletedParameters>(
        'network.responseCompleted',
        0,
      );
      assert.strictEqual(
        getHeader(completedA.response.headers, 'location'),
        RELATIVE_LOCATION,
      );
    });

    it('should emit redirected hop events at once without ExtraInfo', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: false});

      const events = await getHopEvents();
      assert.deepEqual(
        events.map(({method, redirectCount}) => [method, redirectCount]),
        [
          ['network.beforeRequestSent', 0],
          ['network.responseStarted', 0],
          ['network.responseCompleted', 0],
        ],
      );
    });

    it('should stop waiting for missing ExtraInfo when the next hop finishes', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      responseReceived(false);

      assert.deepEqual(await getHopEvents(), []);

      loadingFinished();

      const events = await getHopEvents();
      assert.deepEqual(
        events.map(({method, redirectCount, status}) => [
          method,
          redirectCount,
          status,
        ]),
        [
          ['network.beforeRequestSent', 0, undefined],
          ['network.responseStarted', 0, 302],
          ['network.responseCompleted', 0, 302],
          ['network.beforeRequestSent', 1, undefined],
          ['network.responseStarted', 1, 200],
          ['network.responseCompleted', 1, 200],
        ],
      );
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should not delay an intercepted next hop', async () => {
      const interception = networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: URL_B},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});

      assert.deepEqual(await getHopEvents(), []);

      cdpClient.emit('Fetch.requestPaused', {
        requestId: FETCH_ID,
        networkId: REQUEST_ID,
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'Document',
        request: {
          url: URL_B,
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
      });

      const events = await getHopEvents();
      assert.deepEqual(
        events.map(({method, redirectCount}) => [method, redirectCount]),
        [
          ['network.beforeRequestSent', 0],
          ['network.responseStarted', 0],
          ['network.responseCompleted', 0],
          ['network.beforeRequestSent', 1],
        ],
      );
      assert.deepInclude(getParams('network.beforeRequestSent', 1) as object, {
        isBlocked: true,
        intercepts: [interception],
      });
    });

    it('should apply late request ExtraInfo to the redirected hop when the next hop is auto-continued', async () => {
      // The intercept does not match the hops. The mapper still gets
      // `Fetch.requestPaused` for every request and continues it.
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: URL_C},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      sinon.stub(cdpClient, 'sendCommand').resolves({});
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      cdpClient.emit('Fetch.requestPaused', {
        requestId: FETCH_ID,
        networkId: REQUEST_ID,
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'Document',
        request: {
          url: URL_B,
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
      });
      requestWillBeSentExtraInfo('A', 'John Doe');
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION});
      requestWillBeSentExtraInfo('B', 'Jane Doe');
      responseReceived(true);
      responseReceivedExtraInfo(200);
      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 200],
        ['network.responseCompleted', 1, 'B', 200],
      ]);
      const beforeRequestSentB = getParams<Network.BeforeRequestSentParameters>(
        'network.beforeRequestSent',
        1,
      );
      assert.strictEqual(
        getHeader(beforeRequestSentB.request.headers, 'Cookie'),
        'username=Jane Doe',
      );
    });

    it('should apply late ExtraInfo of two redirects to the correct hops', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      requestWillBeSent(URL_C, {
        from: URL_B,
        hasExtraInfo: true,
        location: RELATIVE_LOCATION_C,
      });
      responseReceived(true, URL_C);

      assert.deepEqual(await getHopEvents(), []);

      requestWillBeSentExtraInfo('A');
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION});

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
      ]);

      requestWillBeSentExtraInfo('B');
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION_C});
      requestWillBeSentExtraInfo('C');
      responseReceivedExtraInfo(200);
      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 302],
        ['network.responseCompleted', 1, 'B', 302],
        ['network.beforeRequestSent', 2, 'C', undefined],
        ['network.responseStarted', 2, 'C', 200],
        ['network.responseCompleted', 2, 'C', 200],
      ]);
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should emit redirected hop events without a missing response ExtraInfo', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      // The response ExtraInfo of the first hop never arrives.
      requestWillBeSentExtraInfo('B');
      responseReceived(true);

      // The first hop waits for its response ExtraInfo before
      // `network.responseCompleted`. The next hop waits for the first hop.
      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
      ]);

      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 200],
      ]);

      responseReceivedExtraInfo(200, {'X-Response': 'B'});

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 200],
        ['network.responseCompleted', 1, 'B', 200],
      ]);
      const completedA = getParams<Network.ResponseCompletedParameters>(
        'network.responseCompleted',
        0,
      );
      assert.isUndefined(getHeader(completedA.response.headers, 'x-response'));
      const completedB = getParams<Network.ResponseCompletedParameters>(
        'network.responseCompleted',
        1,
      );
      assert.strictEqual(
        getHeader(completedB.response.headers, 'x-response'),
        'B',
      );
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should stop waiting for missing ExtraInfo when the next hop fails', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});

      assert.deepEqual(await getHopEvents(), []);

      loadingFailed();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, undefined, undefined],
        ['network.responseStarted', 0, undefined, 302],
        ['network.responseCompleted', 0, undefined, 302],
        ['network.beforeRequestSent', 1, undefined, undefined],
        ['network.fetchError', 1, undefined, undefined],
      ]);
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should stop waiting for missing ExtraInfo when the next hop requires auth', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});

      assert.deepEqual(await getHopEvents(), []);

      // CDP routes `Fetch.authRequired` by the fetch ID, which the request
      // gets from `Fetch.requestPaused`. `Fetch.requestPaused` also stops the
      // wait, so call the handler directly.
      networkStorage.getRequestById(REQUEST_ID)!.onAuthRequired({
        requestId: FETCH_ID,
        request: {
          url: URL_B,
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'Document',
        authChallenge: {
          source: 'Server',
          origin: 'http://localhost',
          scheme: 'basic',
          realm: 'realm',
        },
      });

      const events = await getHopEvents();
      assert.deepEqual(
        events.map(({method, redirectCount}) => [method, redirectCount]),
        [
          ['network.beforeRequestSent', 0],
          ['network.responseStarted', 0],
          ['network.responseCompleted', 0],
          ['network.authRequired', 1],
        ],
      );
    });

    for (const {name, lateExtraInfo} of [
      {name: 'normal', lateExtraInfo: false},
      {name: 'late', lateExtraInfo: true},
    ]) {
      it(`should handle a redirect to the same URL with ${name} ExtraInfo order`, async () => {
        const redirectHeaders = {
          location: RELATIVE_LOCATION_HOME,
          'Set-Cookie': 'session=1',
        };
        requestWillBeSent(URL_HOME);
        if (!lateExtraInfo) {
          requestWillBeSentExtraInfo('A');
          responseReceivedExtraInfo(302, redirectHeaders);
        }
        requestWillBeSent(URL_HOME, {
          from: URL_HOME,
          hasExtraInfo: true,
          location: RELATIVE_LOCATION_HOME,
        });
        if (lateExtraInfo) {
          requestWillBeSentExtraInfo('A');
          responseReceivedExtraInfo(302, redirectHeaders);
        }
        requestWillBeSentExtraInfo('B');
        responseReceived(true, URL_HOME);
        responseReceivedExtraInfo(200, {'X-Response': 'B'});
        loadingFinished();

        assert.deepEqual(summarize(await getHopEvents()), [
          ['network.beforeRequestSent', 0, 'A', undefined],
          ['network.responseStarted', 0, 'A', 302],
          ['network.responseCompleted', 0, 'A', 302],
          ['network.beforeRequestSent', 1, 'B', undefined],
          ['network.responseStarted', 1, 'B', 200],
          ['network.responseCompleted', 1, 'B', 200],
        ]);
        const completedA = getParams<Network.ResponseCompletedParameters>(
          'network.responseCompleted',
          0,
        );
        assert.strictEqual(
          getHeader(completedA.response.headers, 'set-cookie'),
          'session=1',
        );
        const completedB = getParams<Network.ResponseCompletedParameters>(
          'network.responseCompleted',
          1,
        );
        assert.strictEqual(
          getHeader(completedB.response.headers, 'x-response'),
          'B',
        );
        assert.isUndefined(
          getHeader(completedB.response.headers, 'set-cookie'),
        );
        assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
      });
    }

    it('should skip a late redirect response ExtraInfo with a URL fragment in the location', async () => {
      const location = `${RELATIVE_LOCATION}#section`;
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true, location});
      requestWillBeSentExtraInfo('B');
      responseReceived(true);
      // The next hop finishes before the response ExtraInfo of the first hop.
      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 200],
      ]);

      // The late response ExtraInfo of the first hop must not change the
      // status of the next hop.
      responseReceivedExtraInfo(302, {location});
      responseReceivedExtraInfo(200);

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 200],
        ['network.responseCompleted', 1, 'B', 200],
      ]);
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should wait for request ExtraInfo when loadingFinished arrives first without a redirect', async () => {
      // The ExtraInfo events come from the network service. The other events
      // come from the renderer. `loadingFinished` can arrive first.
      requestWillBeSent(URL_A);
      responseReceived(true, URL_A);
      loadingFinished();

      assert.deepEqual(await getHopEvents(), []);

      requestWillBeSentExtraInfo('A', 'John Doe');
      responseReceivedExtraInfo(200);

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 200],
        ['network.responseCompleted', 0, 'A', 200],
      ]);
      const beforeRequestSent = getParams<Network.BeforeRequestSentParameters>(
        'network.beforeRequestSent',
        0,
      );
      assert.lengthOf(beforeRequestSent.request.cookies, 1);
      assert.strictEqual(
        getHeader(beforeRequestSent.request.headers, 'Cookie'),
        'username=John Doe',
      );
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should skip the late response ExtraInfo of two canceled hops', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      requestWillBeSentExtraInfo('B');
      requestWillBeSent(URL_C, {
        from: URL_B,
        hasExtraInfo: true,
        location: RELATIVE_LOCATION_C,
      });
      requestWillBeSentExtraInfo('C');
      responseReceived(true, URL_C);
      // Both redirected hops stop waiting for their response ExtraInfo.
      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 302],
        ['network.responseCompleted', 1, 'B', 302],
        ['network.beforeRequestSent', 2, 'C', undefined],
        ['network.responseStarted', 2, 'C', 200],
      ]);

      // The late ExtraInfo of hop A redirects to hop B, not to hop C.
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION});
      responseReceivedExtraInfo(302, {location: RELATIVE_LOCATION_C});
      responseReceivedExtraInfo(200);

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.responseStarted', 1, 'B', 302],
        ['network.responseCompleted', 1, 'B', 302],
        ['network.beforeRequestSent', 2, 'C', undefined],
        ['network.responseStarted', 2, 'C', 200],
        ['network.responseCompleted', 2, 'C', 200],
      ]);
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should skip the late response ExtraInfo of a canceled hop before a hop that waits', async () => {
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: URL_B},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      sinon.stub(cdpClient, 'sendCommand').resolves({});
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      // Hop B is blocked, so hop A stops waiting for its response ExtraInfo.
      cdpClient.emit('Fetch.requestPaused', {
        requestId: FETCH_ID,
        networkId: REQUEST_ID,
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'Document',
        request: {
          url: URL_B,
          method: 'GET',
          headers: {},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
      });

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, undefined, undefined],
      ]);

      await networkStorage.getRequestById(REQUEST_ID)!.continueRequest();
      requestWillBeSentExtraInfo('B');
      // Hop B redirects before the late response ExtraInfo of hop A arrives.
      // Hop B now waits for its own response ExtraInfo.
      requestWillBeSent(URL_C, {
        from: URL_B,
        hasExtraInfo: true,
        location: RELATIVE_LOCATION_C,
      });
      // The late ExtraInfo of hop A must not go to hop B.
      responseReceivedExtraInfo(302, {
        location: RELATIVE_LOCATION,
        'X-Response': 'A',
      });
      responseReceivedExtraInfo(302, {
        location: RELATIVE_LOCATION_C,
        'X-Response': 'B',
      });
      requestWillBeSentExtraInfo('C');
      responseReceived(true, URL_C);
      responseReceivedExtraInfo(200, {'X-Response': 'C'});
      loadingFinished();

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, undefined, undefined],
        ['network.responseStarted', 1, 'B', 302],
        ['network.responseCompleted', 1, 'B', 302],
        ['network.beforeRequestSent', 2, 'C', undefined],
        ['network.responseStarted', 2, 'C', 200],
        ['network.responseCompleted', 2, 'C', 200],
      ]);
      const completedB = getParams<Network.ResponseCompletedParameters>(
        'network.responseCompleted',
        1,
      );
      assert.strictEqual(
        getHeader(completedB.response.headers, 'x-response'),
        'B',
      );
      const completedC = getParams<Network.ResponseCompletedParameters>(
        'network.responseCompleted',
        2,
      );
      assert.strictEqual(
        getHeader(completedC.response.headers, 'x-response'),
        'C',
      );
      assert.isUndefined(networkStorage.getRequestById(REQUEST_ID));
    });

    it('should use the Fetch.authRequired data in the events emitted when the wait stops', async () => {
      requestWillBeSent(URL_A);
      requestWillBeSentExtraInfo('A');
      requestWillBeSent(URL_B, {from: URL_A, hasExtraInfo: true});
      requestWillBeSentExtraInfo('B');

      // Hop A waits for its response ExtraInfo. Hop B waits for hop A.
      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
      ]);

      networkStorage.getRequestById(REQUEST_ID)!.onAuthRequired({
        requestId: FETCH_ID,
        request: {
          url: URL_B,
          method: 'GET',
          headers: {'X-Auth': 'B'},
          initialPriority: 'VeryHigh',
          referrerPolicy: 'strict-origin-when-cross-origin',
        },
        frameId: MockCdpNetworkEvents.defaultFrameId,
        resourceType: 'Document',
        authChallenge: {
          source: 'Server',
          origin: 'http://localhost',
          scheme: 'basic',
          realm: 'realm',
        },
      });

      assert.deepEqual(summarize(await getHopEvents()), [
        ['network.beforeRequestSent', 0, 'A', undefined],
        ['network.responseStarted', 0, 'A', 302],
        ['network.responseCompleted', 0, 'A', 302],
        ['network.beforeRequestSent', 1, 'B', undefined],
        ['network.authRequired', 1, 'B', -1],
      ]);
      const beforeRequestSentB = getParams<Network.BeforeRequestSentParameters>(
        'network.beforeRequestSent',
        1,
      );
      assert.strictEqual(
        getHeader(beforeRequestSentB.request.headers, 'X-Auth'),
        'B',
      );
    });
  });

  describe('overrides', () => {
    it('should show correct URL for response', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      const overrideUrl = `${request.url}/override`;
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      request.requestWillBeSentRedirect();
      request.responseReceivedExtraInfoRedirect();
      request.requestPaused();
      const networkRequest = networkStorage.getRequestById(request.requestId)!;
      await networkRequest.continueRequest({
        url: overrideUrl,
      });
      request.responseReceived();
      request.responseReceivedExtraInfo();
      request.loadingFinished();

      const event = await getEvent('network.responseCompleted');

      assert.deepNestedInclude(event, {
        'response.url': overrideUrl,
      });
    });
  });

  describe('setCacheBehavior', () => {
    const CONTEXT_1 = 'CONTEXT_1';
    const CONTEXT_2 = 'CONTEXT_2';
    const browsingContext1 = {id: CONTEXT_1} as BrowsingContextImpl;

    it('should return default cache behavior initially', () => {
      assert.equal(networkStorage.getCacheBehavior(CONTEXT_1), 'default');
    });

    it('should update cache behavior for specific contexts', () => {
      networkStorage.setCacheBehavior('bypass', new Set([browsingContext1]));

      assert.equal(networkStorage.getCacheBehavior(CONTEXT_1), 'bypass');
      assert.equal(networkStorage.getCacheBehavior(CONTEXT_2), 'default');
    });

    it('should update default cache behavior and clear context overrides when contexts set is empty', () => {
      networkStorage.setCacheBehavior('bypass', new Set([browsingContext1]));
      networkStorage.setCacheBehavior('default', new Set());

      assert.equal(networkStorage.getCacheBehavior(CONTEXT_1), 'default');
      assert.equal(networkStorage.getCacheBehavior(CONTEXT_2), 'default');

      networkStorage.setCacheBehavior('default', new Set([browsingContext1]));
      networkStorage.setCacheBehavior('bypass', new Set());

      assert.equal(networkStorage.getCacheBehavior(CONTEXT_1), 'bypass');
      assert.equal(networkStorage.getCacheBehavior(CONTEXT_2), 'bypass');
    });
  });

  describe('getCollectedData', () => {
    it('should await final response after redirect', async () => {
      const sendCommandStub = sinon
        .stub(cdpClient, 'sendCommand')
        .resolves({body: 'final body', base64Encoded: false});
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();
      request.requestWillBeSentRedirect();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
      });

      sinon.assert.notCalled(sendCommandStub);

      request.responseReceived();
      request.loadingFinished();

      const result = await dataPromise;
      assert.deepEqual(result, {
        bytes: {type: 'string', value: 'final body'},
      });
      sinon.assert.calledOnceWithExactly(
        sendCommandStub,
        'Network.getResponseBody',
        {requestId: request.requestId},
      );
    });

    it('should reject immediately when no response has been received yet', async () => {
      const sendCommandStub = sinon.stub(cdpClient, 'sendCommand');
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();

      await networkStorage
        .getCollectedData({
          request: request.requestId,
          dataType: Network.DataType.Response,
        })
        .then(
          () => assert.fail('Expected getCollectedData to reject'),
          (error: unknown) =>
            assert.instanceOf(error, NoSuchNetworkDataException),
        );
      sinon.assert.notCalled(sendCommandStub);
    });

    it('should reject when response exceeds maxEncodedDataSize on finish', async () => {
      const sendCommandStub = sinon.stub(cdpClient, 'sendCommand');
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 100,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();
      request.responseReceived();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
      });

      // `loadingFinished` reports `encodedDataLength: 999`, exceeding
      // `maxEncodedDataSize: 100`.
      request.loadingFinished();

      await dataPromise.then(
        () => assert.fail('Expected getCollectedData to reject'),
        (error: unknown) =>
          assert.instanceOf(error, NoSuchNetworkDataException),
      );
      sinon.assert.notCalled(sendCommandStub);
    });

    it('should reject when in-flight request fails and remove uncollected request', async () => {
      const sendCommandStub = sinon.stub(cdpClient, 'sendCommand');
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();
      request.responseReceived();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
      });

      request.loadingFailed();

      await dataPromise.then(
        () => assert.fail('Expected getCollectedData to reject'),
        (error: unknown) =>
          assert.instanceOf(error, NoSuchNetworkDataException),
      );
      sinon.assert.notCalled(sendCommandStub);
      assert.isUndefined(networkStorage.getRequestById(request.requestId));
    });

    it('should wait for responseReceivedExtraInfo when loadingFinished arrives first', async () => {
      sinon
        .stub(cdpClient, 'sendCommand')
        .resolves({body: 'final body', base64Encoded: false});
      const collector = networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();
      request.requestWillBeSentExtraInfo();
      request.responseReceived(true);
      request.loadingFinished();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
        collector,
        disown: true,
      });

      request.responseReceivedExtraInfo();

      const result = await dataPromise;
      assert.deepEqual(result, {
        bytes: {type: 'string', value: 'final body'},
      });
      const event = await getEvent('network.responseCompleted');
      assert.exists(event);
    });

    it('should reject immediately when request is blocked in an intercept phase', async () => {
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestWillBeSent();
      request.requestPaused();

      await networkStorage
        .getCollectedData({
          request: request.requestId,
          dataType: Network.DataType.Response,
        })
        .then(
          () => assert.fail('Expected getCollectedData to reject'),
          (error: unknown) =>
            assert.instanceOf(error, NoSuchNetworkDataException),
        );
    });

    it('should reject when request enters an intercept phase while waiting', async () => {
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: NetworkProcessor.parseUrlPatterns([
          {type: 'string', pattern: request.url},
        ]),
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestWillBeSent();
      request.requestWillBeSentRedirect();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
      });

      request.requestPaused();

      await dataPromise.then(
        () => assert.fail('Expected getCollectedData to reject'),
        (error: unknown) =>
          assert.instanceOf(error, NoSuchNetworkDataException),
      );
    });

    it('should reject immediately on in-flight request when no collectors exist', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();

      await networkStorage
        .getCollectedData({
          request: request.requestId,
          dataType: Network.DataType.Response,
        })
        .then(
          () => assert.fail('Expected getCollectedData to reject'),
          (error: unknown) =>
            assert.instanceOf(error, NoSuchNetworkDataException),
        );
    });

    it('should reject immediately with unknown collector on in-flight request', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();

      await networkStorage
        .getCollectedData({
          request: request.requestId,
          dataType: Network.DataType.Response,
          collector: 'unknown-collector',
        })
        .then(
          () => assert.fail('Expected getCollectedData to reject'),
          (error: unknown) =>
            assert.instanceOf(error, NoSuchNetworkCollectorException),
        );
    });

    it('should keep in-flight request state when disowning request data before finish', async () => {
      sinon
        .stub(cdpClient, 'sendCommand')
        .resolves({postData: 'payload', base64Encoded: false});
      const collector = networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Request, Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });
      // Keeps the request data collected after `collector` disowns it.
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Request],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();

      const result = await networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Request,
        collector,
        disown: true,
      });
      assert.deepEqual(result, {
        bytes: {type: 'string', value: 'payload'},
      });

      // The request is still in-flight, so its URL and state must not be wiped.
      const storedRequest = networkStorage.getRequestById(request.requestId);
      assert.exists(storedRequest);
      assert.equal(storedRequest?.url, request.url);
    });

    it('should unblock pending getCollectedData when target is disposed', async () => {
      const sendCommandStub = sinon.stub(cdpClient, 'sendCommand');
      networkStorage.addDataCollector({
        dataTypes: [Network.DataType.Response],
        maxEncodedDataSize: 10_000,
      });

      const request = new MockCdpNetworkEvents(cdpClient);
      request.requestWillBeSent();
      request.responseReceived();

      const dataPromise = networkStorage.getCollectedData({
        request: request.requestId,
        dataType: Network.DataType.Response,
      });

      networkStorage.disposeRequestMap(cdpClient.sessionId!);

      await dataPromise.then(
        () => assert.fail('Expected getCollectedData to reject'),
        (error: unknown) =>
          assert.instanceOf(error, NoSuchNetworkDataException),
      );
      sinon.assert.notCalled(sendCommandStub);
    });
  });
});
