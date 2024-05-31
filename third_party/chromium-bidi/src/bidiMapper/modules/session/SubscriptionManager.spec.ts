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

    browsingContextStorage.getTopLevelContexts = sinon.stub().callsFake(() => {
      return [{id: SOME_CONTEXT}, {id: ANOTHER_CONTEXT}];
    });

    subscriptionManager = new SubscriptionManager(browsingContextStorage);
  });

  describe('subscribe should return list of added subscriptions', () => {
    describe('specific context', () => {
      it('new subscription', () => {
        expect(
          subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL)
        ).to.deep.equal([{event: SOME_EVENT, contextId: SOME_CONTEXT}]);
      });

      it('existing subscription', () => {
        subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
        expect(
          subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL)
        ).to.deep.equal([]);
      });
    });

    describe('global', () => {
      it('new subscription', () => {
        expect(
          subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL)
        ).to.deep.equal([
          {event: SOME_EVENT, contextId: SOME_CONTEXT},
          {event: SOME_EVENT, contextId: ANOTHER_CONTEXT},
        ]);
      });

      it('existing subscription', () => {
        subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
        expect(
          subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL)
        ).to.deep.equal([{event: SOME_EVENT, contextId: ANOTHER_CONTEXT}]);
      });
    });
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
      expect(
        unrollEvents([ChromiumBidi.BiDiModule.BrowsingContext])
      ).to.have.members([
        ChromiumBidi.BrowsingContext.EventNames.ContextCreated,
        ChromiumBidi.BrowsingContext.EventNames.ContextDestroyed,
        ChromiumBidi.BrowsingContext.EventNames.DomContentLoaded,
        ChromiumBidi.BrowsingContext.EventNames.DownloadWillBegin,
        ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
        ChromiumBidi.BrowsingContext.EventNames.Load,
        ChromiumBidi.BrowsingContext.EventNames.NavigationAborted,
        ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
        ChromiumBidi.BrowsingContext.EventNames.UserPromptClosed,
        ChromiumBidi.BrowsingContext.EventNames.UserPromptOpened,
      ]);
    });

    it('all Log events', () => {
      expect(unrollEvents([ChromiumBidi.BiDiModule.Log])).to.have.members([
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      ]);
    });

    it('all Network events', () => {
      expect(unrollEvents([ChromiumBidi.BiDiModule.Network])).to.have.members([
        ChromiumBidi.Network.EventNames.AuthRequired,
        ChromiumBidi.Network.EventNames.BeforeRequestSent,
        ChromiumBidi.Network.EventNames.FetchError,
        ChromiumBidi.Network.EventNames.ResponseCompleted,
        ChromiumBidi.Network.EventNames.ResponseStarted,
      ]);
    });

    it('all Script events', () => {
      expect(unrollEvents([ChromiumBidi.BiDiModule.Script])).to.have.members([
        ChromiumBidi.Script.EventNames.Message,
        ChromiumBidi.Script.EventNames.RealmCreated,
        ChromiumBidi.Script.EventNames.RealmDestroyed,
      ]);
    });

    it('discrete events', () => {
      expect(
        unrollEvents([
          ChromiumBidi.Script.EventNames.RealmCreated,
          ChromiumBidi.Log.EventNames.LogEntryAdded,
        ])
      ).to.have.members([
        ChromiumBidi.Script.EventNames.RealmCreated,
        ChromiumBidi.Log.EventNames.LogEntryAdded,
      ]);
    });

    it('all and discrete events', () => {
      expect(
        unrollEvents([
          ChromiumBidi.BiDiModule.Log,
          ChromiumBidi.Log.EventNames.LogEntryAdded,
        ])
      ).to.have.members([ChromiumBidi.Log.EventNames.LogEntryAdded]);
    });
  });

  describe('isSubscribedTo', () => {
    describe('module', () => {
      it('should return true global subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          null,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true specific context subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true for module subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.BiDiModule.Network,
          null,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true for nested context when subscribed to top level ancestor context', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.BiDiModule.Network,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            SOME_NESTED_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return false for nested context when subscribed to another context', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.BiDiModule.Network,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            ANOTHER_NESTED_CONTEXT
          )
        ).to.equal(false);
      });

      it('should return false with no subscriptions', () => {
        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.BiDiModule.Network,
            SOME_CONTEXT
          )
        ).to.equal(false);
      });
    });

    describe('event', () => {
      it('should return true global subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          null,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true specific context subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true for module subscription', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.BiDiModule.Network,
          null,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            SOME_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return true for nested context when subscribed to top level ancestor context', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            SOME_NESTED_CONTEXT
          )
        ).to.equal(true);
      });

      it('should return false for nested context when subscribed to another context', () => {
        subscriptionManager.subscribe(
          ChromiumBidi.Network.EventNames.ResponseCompleted,
          SOME_CONTEXT,
          SOME_CHANNEL
        );

        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            ANOTHER_NESTED_CONTEXT
          )
        ).to.equal(false);
      });

      it('should return false with no subscriptions', () => {
        expect(
          subscriptionManager.isSubscribedTo(
            ChromiumBidi.Network.EventNames.ResponseCompleted,
            SOME_CONTEXT
          )
        ).to.equal(false);
      });
    });
  });
});
