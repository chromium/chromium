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
import sinon from 'sinon';

import {
  type BrowsingContext,
  ChromiumBidi,
} from '../../../protocol/protocol.js';
import {BrowsingContextStorage} from '../context/BrowsingContextStorage.js';

import {
  SubscriptionManager,
  cartesianProduct,
  unrollEvents,
} from './SubscriptionManager.js';

const ALL_EVENTS = ChromiumBidi.BiDiModule.BrowsingContext;
const SOME_EVENT = ChromiumBidi.BrowsingContext.EventNames.Load;
const ANOTHER_EVENT = ChromiumBidi.BrowsingContext.EventNames.ContextCreated;
const YET_ANOTHER_EVENT =
  ChromiumBidi.BrowsingContext.EventNames.DomContentLoaded;

const SOME_CONTEXT = 'SOME_CONTEXT';
const SOME_USER_CONTEXT = 'SOME_USER_CONTEXT';
const SOME_NESTED_CONTEXT = 'SOME_NESTED_CONTEXT';
const ANOTHER_CONTEXT = 'ANOTHER_CONTEXT';
const ANOTHER_USER_CONTEXT = 'ANOTHER_USER_CONTEXT';
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
      .callsFake((contextId: BrowsingContext.BrowsingContext) => {
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

    browsingContextStorage.findContext = sinon
      .stub()
      .callsFake((contextId: BrowsingContext.BrowsingContext) => {
        if (contextId === SOME_NESTED_CONTEXT) {
          return {id: SOME_NESTED_CONTEXT, userContext: SOME_USER_CONTEXT};
        }
        if (contextId === SOME_CONTEXT) {
          return {id: SOME_CONTEXT, userContext: SOME_USER_CONTEXT};
        }
        if (contextId === ANOTHER_NESTED_CONTEXT) {
          return {
            id: ANOTHER_NESTED_CONTEXT,
            userContext: ANOTHER_USER_CONTEXT,
          };
        }
        if (contextId === ANOTHER_CONTEXT) {
          return {id: ANOTHER_CONTEXT, userContext: ANOTHER_USER_CONTEXT};
        }
        return undefined;
      });

    browsingContextStorage.getTopLevelContexts = sinon.stub().callsFake(() => {
      return [{id: SOME_CONTEXT}, {id: ANOTHER_CONTEXT}];
    });

    subscriptionManager = new SubscriptionManager(browsingContextStorage);
  });

  describe('getGoogChannelsSubscribedToEvent', () => {
    it('should maintain goog:channel subscription order', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe([SOME_EVENT], [], [], ANOTHER_CHANNEL);
      subscriptionManager.subscribe([ANOTHER_EVENT], [], [], ANOTHER_CHANNEL);
      subscriptionManager.subscribe([ALL_EVENTS], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe([ALL_EVENTS], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe(
        [YET_ANOTHER_EVENT],
        [],
        [],
        ANOTHER_CHANNEL,
      );

      // `SOME_EVENT` was fist subscribed in `SOME_GOOG_CHANNEL`.
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);

      // `ANOTHER_EVENT` was fist subscribed in `ANOTHER_CHANNEL`.
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          ANOTHER_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);

      // `YET_ANOTHER_EVENT` was first subscribed in `SOME_CHANNEL` via
      // `ALL_EVENTS`.
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          YET_ANOTHER_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should re-subscribe in proper order', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe([SOME_EVENT], [], [], ANOTHER_CHANNEL);
      subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);
    });

    it('should subscribe global and specific context in proper order', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe([SOME_EVENT], [], [], ANOTHER_CHANNEL);
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should subscribe specific context and global in proper order', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe([SOME_EVENT], [], [], ANOTHER_CHANNEL);
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should subscribe contexts in proper order', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        ANOTHER_CHANNEL,
      );
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([SOME_CHANNEL, ANOTHER_CHANNEL]);
    });

    it('should re-subscribe contexts in proper order', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        ANOTHER_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.getGoogChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT,
        ),
      ).to.deep.equal([ANOTHER_CHANNEL, SOME_CHANNEL]);
    });
  });

  describe('user-context subscription', () => {
    it('should subscribe to a user context event', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [SOME_USER_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, ANOTHER_CONTEXT),
      ).to.equal(false);
    });

    it('should not unsubscribe by attributes', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [SOME_USER_CONTEXT],
        SOME_CHANNEL,
      );
      expect(() => {
        subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should unsubscribe by id', () => {
      const {id} = subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [SOME_USER_CONTEXT],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribeById([id]);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });
  });

  describe('global subscription', () => {
    it('should subscribe to a global event', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      expect(
        subscriptionManager.isSubscribedTo(ANOTHER_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe by id', () => {
      const {id} = subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribeById([id]);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should not unsubscribe on error', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      expect(() =>
        subscriptionManager.unsubscribe(
          [SOME_EVENT, ANOTHER_EVENT],
          [],
          SOME_CHANNEL,
        ),
      ).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should unsubscribe from multiple subscriptions completely', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe([ANOTHER_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.unsubscribe(
        [SOME_EVENT, ANOTHER_EVENT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
      expect(
        subscriptionManager.isSubscribedTo(ANOTHER_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe from multiple subscriptions partially', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT, ANOTHER_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe(
        [SOME_EVENT, YET_ANOTHER_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
      expect(
        subscriptionManager.isSubscribedTo(ANOTHER_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      expect(
        subscriptionManager.isSubscribedTo(YET_ANOTHER_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should unsubscribe from one subscription partially', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT, ANOTHER_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      expect(() => {
        subscriptionManager.unsubscribe(
          [SOME_EVENT, ANOTHER_EVENT],
          [],
          SOME_CHANNEL,
        );
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
      expect(
        subscriptionManager.isSubscribedTo(ANOTHER_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should not unsubscribe context subscriptions', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.unsubscribe([SOME_EVENT], [], SOME_CHANNEL);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });
  });

  describe('context subscription', () => {
    it('should subscribe per context', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, ANOTHER_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe by id', () => {
      const {id} = subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribeById([id]);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should partially unsubscribe from a context', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT, ANOTHER_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, ANOTHER_CONTEXT),
      ).to.equal(true);
    });

    it('should error if the unsubscribe is invalid', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT, ANOTHER_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      expect(() => {
        subscriptionManager.unsubscribe(
          [SOME_EVENT],
          [SOME_CONTEXT, ANOTHER_CONTEXT],
          SOME_CHANNEL,
        );
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, ANOTHER_CONTEXT),
      ).to.equal(true);
    });

    it('should completely unsubscribe', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe a module', () => {
      subscriptionManager.subscribe(
        [ALL_EVENTS],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [ALL_EVENTS],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should not unsubscribe global subscription', () => {
      subscriptionManager.subscribe([SOME_EVENT], [], [], SOME_CHANNEL);
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should subscribe to top-level context when subscribed to nested context', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_NESTED_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should not subscribe to top-level context when subscribed to nested context of another context', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [ANOTHER_NESTED_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should unsubscribe from top-level context when unsubscribed from nested context', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      subscriptionManager.unsubscribe(
        [SOME_EVENT],
        [SOME_NESTED_CONTEXT],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(false);
    });

    it('should not unsubscribe from top-level context when unsubscribed from nested context in different channel', () => {
      subscriptionManager.subscribe(
        [SOME_EVENT],
        [SOME_CONTEXT],
        [],
        SOME_CHANNEL,
      );
      expect(() => {
        subscriptionManager.unsubscribe(
          [SOME_EVENT],
          [SOME_NESTED_CONTEXT],
          ANOTHER_CHANNEL,
        );
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });
  });

  describe('unsubscribeById', () => {
    it('should keep subscription if one of the IDs is not known', () => {
      const {id} = subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      expect(() => {
        subscriptionManager.unsubscribeById([id, 'wrong']);
      }).to.throw('No subscription found');
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
    });

    it('should throw an error if an ID is not know', () => {
      expect(() => {
        subscriptionManager.unsubscribeById(['wrong']);
      }).to.throw('No subscription found');
    });

    it('should throw an error if a subscription is used multiple times', () => {
      const {id} = subscriptionManager.subscribe(
        [SOME_EVENT],
        [],
        [],
        SOME_CHANNEL,
      );
      expect(
        subscriptionManager.isSubscribedTo(SOME_EVENT, SOME_CONTEXT),
      ).to.equal(true);
      subscriptionManager.unsubscribeById([id]);
      expect(() => {
        subscriptionManager.unsubscribeById([id]);
      }).to.throw('No subscription found');
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
    function unrollEventsToArray(args: any[]) {
      return [...unrollEvents(args)];
    }

    it('all Browsing Context events', () => {
      expect(
        unrollEventsToArray([ChromiumBidi.BiDiModule.BrowsingContext]),
      ).to.have.members([
        ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
        ChromiumBidi.BrowsingContext.EventNames.ContextDestroyed,
        ChromiumBidi.BrowsingContext.EventNames.DomContentLoaded,
        ChromiumBidi.BrowsingContext.EventNames.DownloadWillBegin,
        ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
        ChromiumBidi.BrowsingContext.EventNames.HistoryUpdated,
        ChromiumBidi.BrowsingContext.EventNames.Load,
        ChromiumBidi.BrowsingContext.EventNames.NavigationAborted,
        ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
        ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
        ChromiumBidi.BrowsingContext.EventNames.UserPromptClosed,
        ChromiumBidi.BrowsingContext.EventNames.UserPromptOpened,
      ]);
    });

    it('all Log events', () => {
      expect(
        unrollEventsToArray([ChromiumBidi.BiDiModule.Log]),
      ).to.have.members([ChromiumBidi.Log.EventNames.LogEntryAdded]);
    });

    it('all Network events', () => {
      expect(
        unrollEventsToArray([ChromiumBidi.BiDiModule.Network]),
      ).to.have.members([
        ChromiumBidi.Network.EventNames.AuthRequired,
        ChromiumBidi.Network.EventNames.BeforeRequestSent,
        ChromiumBidi.Network.EventNames.FetchError,
        ChromiumBidi.Network.EventNames.ResponseCompleted,
        ChromiumBidi.Network.EventNames.ResponseStarted,
      ]);
    });

    it('all Script events', () => {
      expect(
        unrollEventsToArray([ChromiumBidi.BiDiModule.Script]),
      ).to.have.members([
        ChromiumBidi.Script.EventNames.Message,
        ChromiumBidi.Script.EventNames.RealmCreated,
        ChromiumBidi.Script.EventNames.RealmDestroyed,
      ]);
    });

    it('discrete events', () => {
      expect(
        unrollEventsToArray([
          ChromiumBidi.Script.EventNames.RealmCreated,
          ChromiumBidi.Log.EventNames.LogEntryAdded,
        ]),
      ).to.have.members([
        ChromiumBidi.Script.EventNames.RealmCreated,
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      ]);
    });

    it('all and discrete events', () => {
      expect(
        unrollEventsToArray([
          ChromiumBidi.BiDiModule.Log,
          ChromiumBidi.Log.EventNames.LogEntryAdded,
        ]),
      ).to.have.members([ChromiumBidi.Log.EventNames.LogEntryAdded]);
    });
  });
});
