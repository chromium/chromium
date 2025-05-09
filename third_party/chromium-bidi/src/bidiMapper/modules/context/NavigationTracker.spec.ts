/*
 *  Copyright 2024 Google LLC.
 *  Copyright (c) Microsoft Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

import {assert} from 'chai';
import sinon, {type SinonStubbedInstance} from 'sinon';

import {ChromiumBidi} from '../../../protocol/protocol.js';
import {EventManager} from '../session/EventManager.js';

import {NavigationEventName, NavigationTracker} from './NavigationTracker.js';

// keep-sorted start block=yes
const ANOTHER_LOADER_ID = 'ANOTHER_LOADER_ID';
const ANOTHER_URL = 'ANOTHER_URL';
const BROWSING_CONTEXT_ID = 'browsingContextId';
const ERROR_MESSAGE = 'ERROR_MESSAGE';
const INITIAL_URL = 'about:blank';
const LOADER_ID = 'LOADER_ID';
const SOME_URL = 'SOME_URL';
const YET_ANOTHER_URL = 'YET_ANOTHER_URL';
// keep-sorted end

describe('NavigationTracker', () => {
  let navigationTracker: NavigationTracker;
  let eventManager: SinonStubbedInstance<EventManager>;
  let initialNavigationId: string;

  function assertNoNavigationEvents() {
    // `eventManager.registerEvent` is safe do be used unbound.

    sinon.assert.notCalled(eventManager.registerEvent);
  }

  function assertNavigationEvent(
    this: void,
    eventName: string,
    navigationId: string | sinon.SinonMatcher,
    url: string,
  ) {
    sinon.assert.calledWith(
      // `eventManager.registerEvent` is safe do be used unbound.

      eventManager.registerEvent,
      sinon.match({
        type: 'event',
        method: eventName,
        params: {
          context: BROWSING_CONTEXT_ID,
          navigation: navigationId,
          timestamp: sinon.match.any,
          url,
        },
      }),
      sinon.match(BROWSING_CONTEXT_ID),
    );
    eventManager.registerEvent.reset();
  }

  beforeEach(() => {
    eventManager = sinon.createStubInstance(EventManager);
    navigationTracker = new NavigationTracker(
      INITIAL_URL,
      BROWSING_CONTEXT_ID,
      eventManager,
    );
    initialNavigationId = navigationTracker.currentNavigationId;
  });

  describe('CDP command initiated navigation', () => {
    it('should process fragment navigation', async () => {
      const navigation = navigationTracker.createPendingNavigation(SOME_URL);
      assert.equal(navigation.url, SOME_URL);
      assert.equal(navigationTracker.url, INITIAL_URL);
      navigationTracker.navigationCommandFinished(navigation, undefined);

      // Assert navigation is not finished.
      assertNoNavigationEvents();

      // Fragment navigation should not update the current navigation.
      assert.equal(navigationTracker.currentNavigationId, initialNavigationId);

      navigationTracker.navigatedWithinDocument(SOME_URL, 'fragment');

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
        navigation.navigationId,
        SOME_URL,
      );
      assert.equal(
        (await navigation.finished).eventName,
        NavigationEventName.FragmentNavigated,
      );
      // Fragment navigation should not update the current navigation.
      assert.equal(navigationTracker.currentNavigationId, initialNavigationId);
      assert.equal(navigationTracker.url, SOME_URL);
    });

    describe('cross-document navigation', () => {
      it('started', async () => {
        const navigation = navigationTracker.createPendingNavigation(SOME_URL);

        assertNoNavigationEvents();
        assert.equal(navigation.url, SOME_URL);
        assert.equal(navigationTracker.url, INITIAL_URL);
        assert.equal(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );

        navigationTracker.frameStartedNavigating(
          ANOTHER_URL,
          LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          navigation.navigationId,
          ANOTHER_URL,
        );
        assert.equal(navigation.url, ANOTHER_URL);
        assert.equal(navigationTracker.url, INITIAL_URL);
        assert.equal(
          navigationTracker.currentNavigationId,
          navigation.navigationId,
        );

        navigationTracker.navigationCommandFinished(navigation, LOADER_ID);

        assertNoNavigationEvents();
        assert.equal(navigation.url, ANOTHER_URL);
        assert.equal(navigationTracker.url, INITIAL_URL);
        assert.equal(
          navigationTracker.currentNavigationId,
          navigation.navigationId,
        );

        navigationTracker.frameNavigated(YET_ANOTHER_URL, LOADER_ID);

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
          navigation.navigationId,
          YET_ANOTHER_URL,
        );

        assert.equal(navigation.url, YET_ANOTHER_URL);
        assert.equal(navigationTracker.url, YET_ANOTHER_URL);
        assert.equal(
          navigationTracker.currentNavigationId,
          navigation.navigationId,
        );

        navigationTracker.loadPageEvent(ANOTHER_LOADER_ID);

        assertNoNavigationEvents();

        navigationTracker.loadPageEvent(LOADER_ID);

        assertNoNavigationEvents();
        assert.equal(
          (await navigation.finished).eventName,
          NavigationEventName.Load,
        );
      });
    });

    it('canceled by script-initiated navigation', async () => {
      const navigation = navigationTracker.createPendingNavigation(SOME_URL);
      navigationTracker.frameStartedNavigating(
        ANOTHER_URL,
        LOADER_ID,
        'differentDocument',
      );

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
        navigation.navigationId,
        ANOTHER_URL,
      );

      navigationTracker.frameStartedNavigating(
        YET_ANOTHER_URL,
        ANOTHER_LOADER_ID,
        'differentDocument',
      );

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        navigation.navigationId,
        ANOTHER_URL,
      );

      assert.equal(
        (await navigation.finished).eventName,
        NavigationEventName.NavigationFailed,
      );
      // Current navigation is one to `YET_ANOTHER_URL`. It should be not the initial one.
      assert.notEqual(
        navigationTracker.currentNavigationId,
        initialNavigationId,
      );
      // The last committed navigation is still the initial one.
      assert.equal(navigationTracker.url, INITIAL_URL);
    });

    it('aborted by script-initiated navigation', async () => {
      const navigation = navigationTracker.createPendingNavigation(SOME_URL);
      navigationTracker.frameStartedNavigating(
        ANOTHER_URL,
        LOADER_ID,
        'differentDocument',
      );
      navigationTracker.frameNavigated(ANOTHER_URL, LOADER_ID);

      eventManager.registerEvent.reset();

      navigationTracker.frameStartedNavigating(
        YET_ANOTHER_URL,
        ANOTHER_LOADER_ID,
        'differentDocument',
      );
      navigationTracker.frameNavigated(YET_ANOTHER_URL, ANOTHER_LOADER_ID);

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationAborted,
        navigation.navigationId,
        ANOTHER_URL,
      );

      assert.equal(
        (await navigation.finished).eventName,
        NavigationEventName.NavigationAborted,
      );
    });

    it('failed command', async () => {
      const navigation = navigationTracker.createPendingNavigation(SOME_URL);
      navigationTracker.frameStartedNavigating(
        ANOTHER_URL,
        LOADER_ID,
        'differentDocument',
      );

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
        navigation.navigationId,
        ANOTHER_URL,
      );

      navigationTracker.failNavigation(navigation, ERROR_MESSAGE);

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        navigation.navigationId,
        ANOTHER_URL,
      );

      assert.equal(
        (await navigation.finished).eventName,
        NavigationEventName.NavigationFailed,
      );
      assert.equal((await navigation.finished).message, ERROR_MESSAGE);
      assert.equal(
        navigationTracker.currentNavigationId,
        navigation.navigationId,
      );
      assert.equal(navigationTracker.url, INITIAL_URL);
    });

    it('failed network', async () => {
      const navigation = navigationTracker.createPendingNavigation(SOME_URL);
      navigationTracker.frameStartedNavigating(
        ANOTHER_URL,
        LOADER_ID,
        'differentDocument',
      );

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
        navigation.navigationId,
        ANOTHER_URL,
      );

      navigationTracker.networkLoadingFailed(LOADER_ID, ERROR_MESSAGE);

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        navigation.navigationId,
        ANOTHER_URL,
      );

      assert.equal(
        (await navigation.finished).eventName,
        NavigationEventName.NavigationFailed,
      );
      assert.equal((await navigation.finished).message, ERROR_MESSAGE);
      assert.equal(
        navigationTracker.currentNavigationId,
        navigation.navigationId,
      );
      assert.equal(navigationTracker.url, INITIAL_URL);
    });
  });

  describe('Renderer initiated navigation', () => {
    it('should process fragment navigation', () => {
      navigationTracker.navigatedWithinDocument(SOME_URL, 'fragment');

      assertNavigationEvent(
        ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
        sinon.match.any,
        SOME_URL,
      );

      assert.equal(navigationTracker.currentNavigationId, initialNavigationId);
      assert.equal(navigationTracker.url, SOME_URL);
    });

    describe('cross-document navigation', () => {
      it('started', () => {
        assertNoNavigationEvents();
        assert.equal(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        assert.equal(navigationTracker.url, INITIAL_URL);

        navigationTracker.frameStartedNavigating(
          ANOTHER_URL,
          LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          sinon.match.any,
          ANOTHER_URL,
        );

        assert.notEqual(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        assert.equal(navigationTracker.url, INITIAL_URL);

        navigationTracker.frameNavigated(YET_ANOTHER_URL, LOADER_ID);

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationCommitted,
          sinon.match.any,
          YET_ANOTHER_URL,
        );

        assert.equal(navigationTracker.url, YET_ANOTHER_URL);

        navigationTracker.loadPageEvent(LOADER_ID);

        assertNoNavigationEvents();
      });

      it('canceled by script-initiated navigation', () => {
        assertNoNavigationEvents();
        assert.equal(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        assert.equal(navigationTracker.url, INITIAL_URL);

        navigationTracker.frameStartedNavigating(
          ANOTHER_URL,
          LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          sinon.match.any,
          ANOTHER_URL,
        );

        navigationTracker.frameStartedNavigating(
          YET_ANOTHER_URL,
          ANOTHER_LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
          sinon.match.any,
          ANOTHER_URL,
        );
        assert.notEqual(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        // The initial navigation is still the last committed one.
        assert.equal(navigationTracker.url, INITIAL_URL);
      });

      it('canceled by command navigation', () => {
        navigationTracker.frameStartedNavigating(
          ANOTHER_URL,
          LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          sinon.match.any,
          ANOTHER_URL,
        );

        navigationTracker.createPendingNavigation(YET_ANOTHER_URL);

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
          sinon.match.any,
          ANOTHER_URL,
        );
        assert.equal(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        assert.equal(navigationTracker.url, INITIAL_URL);
      });

      it('failed network', () => {
        navigationTracker.frameStartedNavigating(
          ANOTHER_URL,
          LOADER_ID,
          'differentDocument',
        );

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          sinon.match.any,
          ANOTHER_URL,
        );

        navigationTracker.networkLoadingFailed(LOADER_ID, ERROR_MESSAGE);

        assertNavigationEvent(
          ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
          sinon.match.any,
          ANOTHER_URL,
        );
        assert.notEqual(
          navigationTracker.currentNavigationId,
          initialNavigationId,
        );
        assert.equal(navigationTracker.url, INITIAL_URL);
      });
    });
  });
});
