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
