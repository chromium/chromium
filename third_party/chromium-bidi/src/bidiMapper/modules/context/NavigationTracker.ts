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

import type {Protocol} from 'devtools-protocol';

import {
  ChromiumBidi,
  UnknownErrorException,
} from '../../../protocol/protocol.js';
import {Deferred} from '../../../utils/Deferred.js';
import {getTimestamp} from '../../../utils/time.js';
import {urlMatchesAboutBlank} from '../../../utils/UrlHelpers.js';
import {uuidv4} from '../../../utils/uuid.js';
import type {EventManager} from '../session/EventManager.js';

class NavigationState {
  readonly navigationId = uuidv4();
  url?: string;

  constructor(url?: string) {
    this.url = url;
  }
}

export class NavigationTracker {
  readonly #eventManager: EventManager;
  readonly #browsingContextId: string;
  #currentNavigation = new NavigationState();
  // When a new navigation is started via `BrowsingContext.navigate` with `wait` set to
  // `None`, the command result should have `navigation` value, but mapper does not have
  // it yet. This value will be set to `navigationId` after next .
  #pendingNavigation?: NavigationState;

  #url: string;
  // The URL of the navigation that is currently in progress. A workaround of the CDP
  // lacking URL for the pending navigation events, e.g. `Page.frameStartedLoading`.
  // Set on `Page.navigate`, `Page.reload` commands, on `Page.frameRequestedNavigation` or
  // on a deprecated `Page.frameScheduledNavigation` event. The latest is required as the
  // `Page.frameRequestedNavigation` event is not emitted for same-document navigations.
  #pendingNavigationUrl: string | undefined;

  // Flags if the initial navigation to `about:blank` is in progress.
  #initialNavigation = true;
  // Flags if the navigation is initiated by `browsingContext.navigate` or
  // `browsingContext.reload` command.
  #navigationInitiatedByCommand = false;

  // Set if there is a pending navigation initiated by `BrowsingContext.navigate` command.
  // The promise is resolved when the navigation is finished or rejected when canceled.
  #pendingCommandNavigation: Deferred<void> | undefined;

  navigation = {
    withinDocument: new Deferred<void>(),
  };

  constructor(
    url: string,
    browsingContextId: string,
    eventManager: EventManager,
  ) {
    this.#browsingContextId = browsingContextId;
    this.#url = url;
    this.#eventManager = eventManager;
  }

  get currentNavigationId() {
    return this.#currentNavigation.navigationId;
  }

  get initialNavigation(): boolean {
    return this.#initialNavigation;
  }

  get pendingCommandNavigation(): Deferred<void> | undefined {
    return this.#pendingCommandNavigation;
  }

  get url(): string {
    return this.#url;
  }

  dispose() {
    this.#pendingCommandNavigation?.reject(
      new UnknownErrorException('navigation canceled by context disposal'),
    );
  }

  onTargetInfoChanged(url: string) {
    this.#url = url;
  }

  frameNavigated(url: string) {
    this.#url = url;
    this.#pendingNavigationUrl = undefined;
  }

  navigatedWithinDocument(
    url: string,
    navigationType: Protocol.Page.NavigatedWithinDocumentEvent['navigationType'],
  ) {
    this.#pendingNavigationUrl = undefined;
    const timestamp = getTimestamp();
    this.#url = url;
    this.navigation.withinDocument.resolve();

    if (navigationType === 'fragment') {
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.FragmentNavigated,
          params: {
            context: this.#browsingContextId,
            navigation: this.#currentNavigation.navigationId,
            timestamp,
            url: this.#url,
          },
        },
        this.#browsingContextId,
      );
    }
  }

  frameStartedLoading() {
    if (this.#navigationInitiatedByCommand) {
      // In case of the navigation is initiated by `browsingContext.navigate` or
      // `browsingContext.reload` commands, the `Page.frameRequestedNavigation` is not
      // emitted, which means the `NavigationStarted` is not emitted.
      // TODO: consider emit it right after the CDP command `navigate` or `reload` is finished.

      // The URL of the navigation that is currently in progress. Although the URL
      // is not yet known in case of user-initiated navigations, it is possible to
      // provide the URL in case of BiDi-initiated navigations.
      // TODO: provide proper URL in case of user-initiated navigations.
      const url = this.#pendingNavigationUrl ?? 'UNKNOWN';
      this.#currentNavigation =
        this.#pendingNavigation ?? new NavigationState();
      this.#pendingNavigation = undefined;
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          params: {
            context: this.#browsingContextId,
            navigation: this.#currentNavigation.navigationId,
            timestamp: getTimestamp(),
            url,
          },
        },
        this.#browsingContextId,
      );
    }
  }

  frameScheduledNavigation(url: string) {
    this.#pendingNavigationUrl = url;
  }

  frameRequestedNavigation(url: string) {
    if (this.#pendingCommandNavigation !== undefined) {
      // The pending navigation was aborted by the new one.
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.NavigationAborted,
          params: {
            context: this.#browsingContextId,
            navigation: this.#currentNavigation.navigationId,
            timestamp: getTimestamp(),
            url: this.#url,
          },
        },
        this.#browsingContextId,
      );
      this.#pendingCommandNavigation.reject(
        new UnknownErrorException('navigation aborted'),
      );
      this.#pendingCommandNavigation = undefined;
      this.#navigationInitiatedByCommand = false;
    }
    if (!urlMatchesAboutBlank(url)) {
      // If the url does not match about:blank, do not consider it is an initial
      // navigation and emit all the required events.
      // https://github.com/GoogleChromeLabs/chromium-bidi/issues/2793.
      this.#initialNavigation = false;
    }

    if (!this.#initialNavigation) {
      // Do not emit the event for the initial navigation to `about:blank`.
      this.#currentNavigation =
        this.#pendingNavigation ?? new NavigationState();
      this.#pendingNavigation = undefined;
      this.#eventManager.registerEvent(
        {
          type: 'event',
          method: ChromiumBidi.BrowsingContext.EventNames.NavigationStarted,
          params: {
            context: this.#browsingContextId,
            navigation: this.#currentNavigation.navigationId,
            timestamp: getTimestamp(),
            url,
          },
        },
        this.#browsingContextId,
      );
    }

    this.#pendingNavigationUrl = url;
  }

  navigationFinishedWithinSameDocument() {
    if (this.navigation.withinDocument.isFinished) {
      this.navigation.withinDocument = new Deferred();
    }
  }

  lifecycleEventLoad() {
    this.#initialNavigation = false;
  }

  createCommandNavigation(url: string): NavigationState {
    this.#pendingCommandNavigation?.reject(
      new UnknownErrorException('navigation canceled by concurrent navigation'),
    );
    // Set the pending navigation URL to provide it in `browsingContext.navigationStarted`
    // event.
    // TODO: detect navigation start not from CDP. Check if
    //  `Page.frameRequestedNavigation` can be used for this purpose.
    this.#pendingNavigationUrl = url;
    const navigation = new NavigationState(url);
    this.#pendingNavigation = navigation;
    this.#pendingCommandNavigation = new Deferred<void>();
    this.#navigationInitiatedByCommand = true;

    return navigation;
  }

  failCommandNavigation(navigation: NavigationState) {
    // If navigation failed, no pending navigation is left.
    this.#pendingNavigationUrl = undefined;
    this.#eventManager.registerEvent(
      {
        type: 'event',
        method: ChromiumBidi.BrowsingContext.EventNames.NavigationFailed,
        params: {
          context: this.#browsingContextId,
          navigation: this.#currentNavigation.navigationId,
          timestamp: getTimestamp(),
          url: navigation.url ?? 'UNKNOWN',
        },
      },
      this.#browsingContextId,
    );
  }

  finishCommandNavigation(
    navigation: NavigationState,
    finishedByWaitNone: boolean,
  ) {
    // `#pendingCommandNavigation` can be already rejected and set to undefined.
    this.#pendingCommandNavigation?.resolve();
    if (!finishedByWaitNone) {
      this.#navigationInitiatedByCommand = false;
    }
    this.#pendingCommandNavigation = undefined;
  }
}
