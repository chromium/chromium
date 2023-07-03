/**
 * Copyright 2022 Google LLC.
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

import {expect} from 'chai';
import * as sinon from 'sinon';

import {
  BrowsingContext,
  type CommonDataTypes,
  Log,
  Network,
  Script,
} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from '../context/browsingContextStorage.js';

import {
  SubscriptionManager,
  cartesianProduct,
  unrollEvents,
} from './SubscriptionManager.js';

const ALL_EVENTS = BrowsingContext.AllEvents;
const SOME_EVENT = BrowsingContext.EventNames.LoadEvent;
const ANOTHER_EVENT = BrowsingContext.EventNames.ContextCreatedEvent;
const YET_ANOTHER_EVENT = BrowsingContext.EventNames.DomContentLoadedEvent;

const SOME_CONTEXT = 'SOME_CONTEXT';
const SOME_NESTED_CONTEXT = 'SOME_NESTED_CONTEXT';
const ANOTHER_CONTEXT = 'ANOTHER_CONTEXT';
const ANOTHER_NESTED_CONTEXT = 'ANOTHER_NESTED_CONTEXT';

const SOME_CHANNEL = 'SOME_CHANNEL';
const ANOTHER_CHANNEL = 'ANOTHER_CHANNEL';

describe('SubscriptionManager', () => {
  let subscriptionManager: SubscriptionManager;

  beforeEach(() => {
    const browsingContextStorage: BrowsingContextStorage =
      sinon.createStubInstance(BrowsingContextStorage);
    browsingContextStorage.findTopLevelContextId = sinon
      .stub()
      .callsFake((contextId: CommonDataTypes.BrowsingContext) => {
        if (contextId === SOME_NESTED_CONTEXT) {
          return SOME_CONTEXT;
        }
        if (contextId === SOME_CONTEXT) {
          return SOME_CONTEXT;
        }
        if (contextId === ANOTHER_NESTED_CONTEXT) {
          return ANOTHER_CONTEXT;
        }
        if (contextId === ANOTHER_CONTEXT) {
          return ANOTHER_CONTEXT;
        }
        return null;
      });

    subscriptionManager = new SubscriptionManager(browsingContextStorage);
  });

  it('should subscribe twice to global and specific event in proper order', () => {
    subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
    subscriptionManager.subscribe(SOME_EVENT, null, ANOTHER_CHANNEL);
    subscriptionManager.subscribe(ANOTHER_EVENT, null, ANOTHER_CHANNEL);
    subscriptionManager.subscribe(ALL_EVENTS, null, SOME_CHANNEL);
    subscriptionManager.subscribe(ALL_EVENTS, null, SOME_CHANNEL);
    subscriptionManager.subscribe(YET_ANOTHER_EVENT, null, ANOTHER_CHANNEL);

    // `SOME_EVENT` was fist subscribed in `SOME_CHANNEL`.
    expect(
      subscriptionManager.getChannelsSubscribedToEvent(SOME_EVENT, SOME_CONTEXT)
    ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);

    // `ANOTHER_EVENT` was fist subscribed in `ANOTHER_CHANNEL`.
    expect(
      subscriptionManager.getChannelsSubscribedToEvent(
        ANOTHER_EVENT,
        SOME_CONTEXT
      )
    ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);

    // `YET_ANOTHER_EVENT` was first subscribed in `SOME_CHANNEL` via
    // `ALL_EVENTS`.
    expect(
      subscriptionManager.getChannelsSubscribedToEvent(
        YET_ANOTHER_EVENT,
        SOME_CONTEXT
      )
    ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
  });

  describe('with null context', () => {
    it('should send proper event in any context', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });

    it('should not send wrong event', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          ANOTHER_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should unsubscribe', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    describe('unsubscribe all', () => {
      it('atomicity: does not unsubscribe when there is no subscription', () => {
        subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
        expect(() =>
          subscriptionManager.unsubscribeAll(
            [SOME_EVENT, ANOTHER_EVENT],
            [null],
            SOME_CHANNEL
          )
        ).to.throw('No subscription found');
        expect(
          subscriptionManager.getChannelsSubscribedToEvent(
            SOME_EVENT,
            SOME_CONTEXT
          )
        ).to.deep.equal([SOME_CHANNEL]);
      });

      it('happy path', () => {
        subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
        subscriptionManager.subscribe(ANOTHER_EVENT, null, SOME_CHANNEL);
        subscriptionManager.unsubscribeAll(
          [SOME_EVENT, ANOTHER_EVENT],
          [null],
          SOME_CHANNEL
        );
        expect(
          subscriptionManager.getChannelsSubscribedToEvent(
            SOME_EVENT,
            SOME_CONTEXT
          )
        ).to.deep.equal([]);
        expect(
          subscriptionManager.getChannelsSubscribedToEvent(
            ANOTHER_EVENT,
            SOME_CONTEXT
          )
        ).to.deep.equal([]);
      });
    });

    it('should not unsubscribe specific context subscription', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });

    it('should subscribe in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, ANOTHER_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should re-subscribe in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, ANOTHER_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);
    });

    it('should re-subscribe global and specific context in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, ANOTHER_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });
  });

  describe('with some context', () => {
    it('should send proper event in proper context', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });

    it('should not send proper event in wrong context', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          ANOTHER_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should not send wrong event in proper context', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          ANOTHER_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should unsubscribe', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should unsubscribe the domain', () => {
      subscriptionManager.subscribe(ALL_EVENTS, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(ALL_EVENTS, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should not unsubscribe the domain if not subscribed', () => {
      subscriptionManager.subscribe(ALL_EVENTS, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(ALL_EVENTS, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should not unsubscribe global subscription', () => {
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });

    it('should subscribe in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, ANOTHER_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should re-subscribe in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, ANOTHER_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);
    });

    it('should re-subscribe global and specific context in proper order', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, ANOTHER_CHANNEL);
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });
  });

  describe('with nested contexts', () => {
    it('should subscribe to top-level context when subscribed to nested context', () => {
      subscriptionManager.subscribe(
        SOME_EVENT,
        SOME_NESTED_CONTEXT,
        SOME_CHANNEL
      );
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });

    it('should not subscribe to top-level context when subscribed to nested context of another context', () => {
      subscriptionManager.subscribe(
        SOME_EVENT,
        ANOTHER_NESTED_CONTEXT,
        SOME_CHANNEL
      );
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should unsubscribe from top-level context when unsubscribed from nested context', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(
        SOME_EVENT,
        SOME_NESTED_CONTEXT,
        SOME_CHANNEL
      );
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });

    it('should not unsubscribe from top-level context when unsubscribed from nested context in different channel', () => {
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(() => {
        subscriptionManager.unsubscribe(
          SOME_EVENT,
          SOME_NESTED_CONTEXT,
          ANOTHER_CHANNEL
        );
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });
  });

  describe('cartesian product', () => {
    it('should return empty array for empty array', () => {
      expect(cartesianProduct([], [])).to.deep.equal([]);
    });

    it('works with a single input', () => {
      expect(cartesianProduct([1n, 2n])).to.deep.equal([1n, 2n]);
    });

    it('works with multiple inputs', () => {
      expect(cartesianProduct([1], [2], [3])).to.deep.equal([[1, 2, 3]]);
    });

    it('happy path', () => {
      expect(cartesianProduct([1, 2], ['A', 'B'])).to.deep.equal([
        [1, 'A'],
        [1, 'B'],
        [2, 'A'],
        [2, 'B'],
      ]);
    });
  });

  describe('unroll events', () => {
    it('all Browsing Context events', () => {
      expect(unrollEvents([BrowsingContext.AllEvents])).to.have.members([
        BrowsingContext.EventNames.ContextCreatedEvent,
        BrowsingContext.EventNames.ContextDestroyedEvent,
        BrowsingContext.EventNames.DomContentLoadedEvent,
        BrowsingContext.EventNames.FragmentNavigated,
        BrowsingContext.EventNames.LoadEvent,
        BrowsingContext.EventNames.NavigationStarted,
        BrowsingContext.EventNames.UserPromptClosed,
        BrowsingContext.EventNames.UserPromptOpened,
      ]);
    });

    it('all Log events', () => {
      expect(unrollEvents([Log.AllEvents])).to.have.members([
        Log.EventNames.LogEntryAddedEvent,
      ]);
    });

    it('all Network events', () => {
      expect(unrollEvents([Network.AllEvents])).to.have.members([
        Network.EventNames.BeforeRequestSentEvent,
        Network.EventNames.FetchErrorEvent,
        Network.EventNames.ResponseCompletedEvent,
        Network.EventNames.ResponseStartedEvent,
      ]);
    });

    it('all Script events', () => {
      expect(unrollEvents([Script.AllEvents])).to.have.members([
        Script.EventNames.MessageEvent,
        Script.EventNames.RealmCreated,
        Script.EventNames.RealmDestroyed,
      ]);
    });

    it('discrete events', () => {
      expect(
        unrollEvents([
          Script.EventNames.RealmCreated,
          Log.EventNames.LogEntryAddedEvent,
        ])
      ).to.have.members([
        Script.EventNames.RealmCreated,
        Log.EventNames.LogEntryAddedEvent,
      ]);
    });

    it('all and discrete events', () => {
      expect(
        unrollEvents([Log.AllEvents, Log.EventNames.LogEntryAddedEvent])
      ).to.have.members([Log.EventNames.LogEntryAddedEvent]);
    });
  });
});
