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

import type {CdpClient} from '../../../cdp/CdpClient.js';
import {ChromiumBidi, Network} from '../../../protocol/protocol.js';
import {ProcessingQueue} from '../../../utils/ProcessingQueue.js';
import type {OutgoingMessage} from '../../OutgoingMessage.js';
import type {CdpTarget} from '../cdp/CdpTarget.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';
import {EventManager, EventManagerEvents} from '../session/EventManager.js';

import {
  MockCdpNetworkEvents,
  MockCdpTarget,
} from './NetworkModuleMocks.spec.js';
import {NetworkStorage} from './NetworkStorage.js';

function logger(...args: any[]) {
  // eslint-disable-next-line no-constant-condition
  if (false) {
    // eslint-disable-next-line no-console
    console.log(...args);
  }
}

describe('NetworkStorage', () => {
  let processedEvents: [
    ChromiumBidi.Event['method'],
    ChromiumBidi.Event['params'],
  ][] = [];
  let eventManager!: EventManager;
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
    const browsingContextStorage = new BrowsingContextStorage();
    const cdpTarget = new MockCdpTarget(logger) as unknown as CdpTarget;
    const browsingContext = {
      cdpTarget,
      id: MockCdpNetworkEvents.defaultFrameId,
    } as unknown as BrowsingContextImpl;
    cdpClient = cdpTarget.cdpClient;
    // We need to add it the storage to emit properly
    browsingContextStorage.addContext(browsingContext);
    eventManager = new EventManager(browsingContextStorage);
    processingQueue = new ProcessingQueue<OutgoingMessage>(
      async ({message}) => {
        if (message.type === 'event') {
          processedEvents.push([message.method, message.params]);
        }
        return await Promise.resolve();
      },
      logger
    );
    // Subscribe to the `network` module globally
    eventManager.subscriptionManager.subscribe(
      ChromiumBidi.BiDiModule.Network,
      // Verify that the Request send the message
      // To the correct context
      MockCdpNetworkEvents.defaultFrameId,
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
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: request.url}],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

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
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: request.url}],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

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
      networkStorage.addIntercept({
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

    it('should work with non blocking interception and fail response', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);
      networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: 'http://not.correct.com'}],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });

      request.requestWillBeSent();
      request.requestPaused();
      let event = await getEvent('network.beforeRequestSent');
      expect(event).to.not.exist;

      request.loadingFailed();
      event = await getEvent('network.beforeRequestSent');
      expect(event).to.deep.include({
        isBlocked: false,
      });
    });

    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      const event = await getEvent('network.beforeRequestSent');
      expect(event).to.exist;
    });

    it('should work with data url and global interception', async () => {
      networkStorage.addIntercept({
        urlPatterns: [{type: 'pattern'}],
        phases: [Network.InterceptPhase.BeforeRequestSent],
      });
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      const event = await getEvent('network.beforeRequestSent');
      expect(event).to.exist;
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
      const request = new MockCdpNetworkEvents(cdpClient);
      const interception = networkStorage.addIntercept({
        urlPatterns: [{type: 'string', pattern: request.url}],
        phases: [Network.InterceptPhase.ResponseStarted],
      });

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
      networkStorage.addIntercept({
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

    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.responseReceived();
      const event = await getEvent('network.responseStarted');
      expect(event).to.exist;
    });
  });

  describe('network.authRequired', () => {
    it('should work for normal order', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.requestPaused();
      request.requestWillBeSentExtraInfo();
      request.authRequired();
      const event = await getEvent('network.authRequired');
      expect(event).to.deep.nested.include({
        'request.request': request.requestId,
        'request.method': 'GET',
      });
    });

    it('should work with only authRequired', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.authRequired();
      const event = await getEvent('network.authRequired');
      event;
      expect(event).to.deep.nested.include({
        'request.request': request.fetchId,
        'request.method': 'GET',
      });
    });

    it('should work report multiple authRequired', async () => {
      const request = new MockCdpNetworkEvents(cdpClient);

      request.authRequired();
      let events = await getEvents('network.authRequired');
      expect(events).to.have.length(1);
      request.authRequired();
      events = await getEvents('network.authRequired');
      expect(events).to.have.length(2);
    });
  });

  describe('network.responseCompleted', () => {
    it('should work with data url', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.responseReceived();
      const event = await getEvent('network.responseCompleted');
      expect(event).to.exist;
    });

    it('should work with redirect', async () => {
      const request = new MockCdpNetworkEvents(cdpClient, {
        url: 'data:text/html,<div>yo</div>',
      });

      request.requestWillBeSent();
      request.requestWillBeSentRedirect();
      const event = await getEvent('network.responseCompleted');
      expect(event).to.exist;
    });
  });
});
