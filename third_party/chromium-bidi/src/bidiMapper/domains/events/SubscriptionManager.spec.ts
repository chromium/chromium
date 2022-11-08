/**
 * Copyright 2022 Google LLC.
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

import * as chai from 'chai';
import {SubscriptionManager} from './SubscriptionManager';

const expect = chai.expect;
const SOME_EVENT = 'SOME_EVENT';
const ANOTHER_EVENT = 'ANOTHER_EVENT';
const SOME_CONTEXT = 'SOME_CONTEXT';
const ANOTHER_CONTEXT = 'ANOTHER_CONTEXT';
const SOME_CHANNEL = 'SOME_CHANNEL';
const ANOTHER_CHANNEL = 'ANOTHER_CHANNEL';

describe('test SubscriptionManager', () => {
  describe('with null context', () => {
    it('should send proper event in any context', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });
    it('should not send wrong event', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          ANOTHER_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });
    it('should unsubscribe', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, null, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, null, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });
    it('should not unsubscribe specific context subscription', () => {
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([SOME_CHANNEL]);
    });
    it('should not send proper event in wrong context', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          ANOTHER_CONTEXT
        )
      ).to.deep.equal([]);
    });
    it('should not send wrong event in proper context', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          ANOTHER_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });
    it('should unsubscribe', () => {
      const subscriptionManager = new SubscriptionManager();
      subscriptionManager.subscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      subscriptionManager.unsubscribe(SOME_EVENT, SOME_CONTEXT, SOME_CHANNEL);
      expect(
        subscriptionManager.getChannelsSubscribedToEvent(
          SOME_EVENT,
          SOME_CONTEXT
        )
      ).to.deep.equal([]);
    });
    it('should not unsubscribe global subscription', () => {
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
      const subscriptionManager = new SubscriptionManager();
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
});
