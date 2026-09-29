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

describe('EventManager', () => {
  let eventManager: EventManager;

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

    eventManager = new EventManager(browsingContextStorage, userContextStorage);
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
});
