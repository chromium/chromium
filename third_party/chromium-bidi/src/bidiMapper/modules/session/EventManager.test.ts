/**
 * Copyright 2026 Google LLC.
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

import {beforeEach, describe, it} from 'node:test';

import {assert} from 'chai';
import sinon from 'sinon';

import {
  type BrowsingContext,
  ChromiumBidi,
  Log,
} from '../../../protocol/protocol.js';
import type {Result} from '../../../utils/result.js';
import {UserContextStorage} from '../browser/UserContextStorage.js';
import type {BrowsingContextImpl} from '../context/BrowsingContextImpl.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {AclManager} from './AclManager.js';
import {EventManager, EventManagerEvents} from './EventManager.js';

const SOME_CONTEXT = 'SOME_CONTEXT';
const SOME_USER_CONTEXT = 'default';
const SOME_CHANNEL = 'SOME_CHANNEL';
const ANOTHER_CHANNEL = 'ANOTHER_CHANNEL';

const LOG_EVENT: ChromiumBidi.Event = {
  type: 'event',
  method: ChromiumBidi.Log.EventNames.LogEntryAdded,
  params: {
    level: Log.Level.Info,
    source: {
      realm: 'SOME_REALM',
      context: SOME_CONTEXT,
    },
    text: 'SOME_TEXT',
    timestamp: 0,
    type: 'console',
    method: 'log',
    args: [],
  },
};

const CONTEXT_CREATED_EVENT = {
  type: 'event',
  method: ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
  params: {context: SOME_CONTEXT},
} as unknown as ChromiumBidi.Event;

const NAVIGATION_COMMITTED_EVENT = {
  type: 'event',
  method: ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
  params: {context: SOME_CONTEXT},
} as unknown as ChromiumBidi.Event;

describe('EventManager', () => {
  let eventManager: EventManager;
  let aclManager: AclManager;

  beforeEach(() => {
    const browsingContextStorage = new BrowsingContextStorage();
    const context = {
      id: SOME_CONTEXT,
      userContext: SOME_USER_CONTEXT,
      isTopLevelContext: () => true,
      toggleModulesIfNeeded: () => Promise.resolve(),
    } as unknown as BrowsingContextImpl;
    browsingContextStorage.addContext(context);
    browsingContextStorage.findTopLevelContextId = sinon
      .stub()
      .callsFake((contextId: BrowsingContext.BrowsingContext) =>
        contextId === SOME_CONTEXT ? SOME_CONTEXT : null,
      );

    const userContextStorage = sinon.createStubInstance(UserContextStorage);
    userContextStorage.verifyUserContextIdList.resolves();

    aclManager = new AclManager(browsingContextStorage);
    eventManager = new EventManager(
      browsingContextStorage,
      userContextStorage,
      aclManager,
    );
  });

  describe('registerPromiseEvent', () => {
    it('should not evaluate lazy event until subscribed', async () => {
      const eventFactory = sinon.spy((): Promise<Result<ChromiumBidi.Event>> =>
        Promise.resolve({
          kind: 'success',
          value: LOG_EVENT,
        }),
      );

      eventManager.registerPromiseEvent(
        eventFactory,
        SOME_CONTEXT,
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      );

      sinon.assert.notCalled(eventFactory);

      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );

      sinon.assert.calledOnce(eventFactory);

      // Subscribing on another channel should reuse the cached promise.
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [SOME_CONTEXT],
        [],
        ANOTHER_CHANNEL,
      );

      sinon.assert.calledOnce(eventFactory);
    });

    it('should evaluate lazy event once when already subscribed on multiple channels', async () => {
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [SOME_CONTEXT],
        [],
        ANOTHER_CHANNEL,
      );

      const eventFactory = sinon.spy((): Promise<Result<ChromiumBidi.Event>> =>
        Promise.resolve({
          kind: 'success',
          value: LOG_EVENT,
        }),
      );

      const emittedMessages: Promise<any>[] = [];
      eventManager.on(EventManagerEvents.Event, ({message}) => {
        emittedMessages.push(message);
      });

      eventManager.registerPromiseEvent(
        eventFactory,
        SOME_CONTEXT,
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      );

      sinon.assert.calledOnce(eventFactory);
      assert.lengthOf(emittedMessages, 2);
    });
  });

  describe('in restricted sessions', () => {
    let emittedEvents: string[];

    beforeEach(() => {
      aclManager.setEnabled(true);
      emittedEvents = [];
      eventManager.on(EventManagerEvents.Event, ({event}) => {
        emittedEvents.push(event);
      });
    });

    it('allows subscribing to modules and sends only events in the ACL', async () => {
      aclManager.setAcl({
        default: {
          events: [ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted],
        },
      });
      await eventManager.subscribe(
        [ChromiumBidi.BiDiModule.BrowsingContext],
        [],
        [],
        SOME_CHANNEL,
      );

      eventManager.registerEvent(CONTEXT_CREATED_EVENT, SOME_CONTEXT);
      eventManager.registerEvent(NAVIGATION_COMMITTED_EVENT, SOME_CONTEXT);

      assert.deepEqual(emittedEvents, [
        ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
      ]);
    });

    it('applies ACL changes to existing subscriptions', async () => {
      await eventManager.subscribe(
        [ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted],
        [],
        [],
        SOME_CHANNEL,
      );

      eventManager.registerEvent(NAVIGATION_COMMITTED_EVENT, SOME_CONTEXT);
      assert.isEmpty(emittedEvents);

      aclManager.setAcl({
        perNavigable: {
          [SOME_CONTEXT]: {
            events: [
              ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
            ],
          },
        },
      });
      eventManager.registerEvent(NAVIGATION_COMMITTED_EVENT, SOME_CONTEXT);
      assert.deepEqual(emittedEvents, [
        ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
      ]);
    });

    it('sends events without a browsing context only if the default ACL allows them', async () => {
      const event = ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted;
      await eventManager.subscribe([event], [], [], SOME_CHANNEL);

      aclManager.setAcl({perNavigable: {[SOME_CONTEXT]: {events: [event]}}});
      eventManager.registerGlobalEvent(NAVIGATION_COMMITTED_EVENT);
      assert.isEmpty(emittedEvents);

      aclManager.setAcl({default: {events: [event]}});
      eventManager.registerGlobalEvent(NAVIGATION_COMMITTED_EVENT);
      assert.deepEqual(emittedEvents, [event]);
    });

    it('does not evaluate lazy events that the ACL withholds', async () => {
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [],
        [],
        SOME_CHANNEL,
      );
      const eventFactory = sinon.spy((): Promise<Result<ChromiumBidi.Event>> =>
        Promise.resolve({kind: 'success', value: LOG_EVENT}),
      );

      eventManager.registerPromiseEvent(
        eventFactory,
        SOME_CONTEXT,
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      );

      sinon.assert.notCalled(eventFactory);
      assert.isEmpty(emittedEvents);
    });

    it('does not replay buffered events that the ACL withheld', async () => {
      eventManager.registerEvent(LOG_EVENT, SOME_CONTEXT);
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [],
        [],
        SOME_CHANNEL,
      );
      assert.isEmpty(emittedEvents);

      aclManager.setAcl({
        default: {events: [ChromiumBidi.Log.EventNames.LogEntryAdded]},
      });
      await eventManager.subscribe(
        [ChromiumBidi.Log.EventNames.LogEntryAdded],
        [],
        [],
        SOME_CHANNEL,
      );
      assert.isEmpty(emittedEvents);

      // New events are sent once the ACL allows them, once per channel.
      eventManager.registerEvent(LOG_EVENT, SOME_CONTEXT);
      assert.deepEqual(emittedEvents, [
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      ]);
    });
  });
});
