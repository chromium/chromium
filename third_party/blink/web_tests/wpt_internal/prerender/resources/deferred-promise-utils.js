// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This file co-works with a html file to test a promise that should be deferred
 * during prerendering.
 *
 * Usage example:
 *  Suppose the html is "prerender-promise-test.html"
 *  On prerendering page, prerender-promise-test.html?prerendering:
 *    const promise = {a promise that should be deferred during prerendering};
 *    const prerenderEventCollector = new PrerenderEventCollector();
 *    prerenderEventCollector.start(promise, {promise name});
 *
 *  On the initiator page, prerender-promise-test.html:
 *   execute
 *    `loadInitiatorPage();`
 */

import('./utils.js');

// Collects events that happen relevant to a prerendering page.
// An event is added when:
// 1. start() is called.
// 2. a prerenderingchange event is dispatched on this document.
// 3. the promise passed to start() is resolved.
// 4. addEvent() is called manually.
class PrerenderEventCollector {
  constructor() {
    // Used to communicate with the initiator page.
    this.prerenderChannel_ = new BroadcastChannel('prerender-channel');
    // Used to communicate with the main test page.
    this.testChannel_ = new BroadcastChannel('test-channel');
    this.eventsSeen_ = [];
  }

  // Adds an event to `eventsSeen_` along with the prerendering state of the
  // page.
  addEvent(eventMessage) {
    this.eventsSeen_.push(
        {event: eventMessage, prerendering: document.prerendering});
  }

  // Starts collecting events until the promise resolves.
  async start(promise, promiseName) {
    assert_true(document.prerendering);
    this.addEvent(`started waiting ${promiseName}`);
    promise.then(() => {
      this.addEvent(`finished waiting ${promiseName}`);
      // Send the observed events back to the main test page.
      this.testChannel_.postMessage(this.eventsSeen_);
      this.prerenderChannel_.close();
      this.testChannel_.close();
      window.close();
    });
    document.addEventListener('prerenderingchange', () => {
      this.addEvent('prerendering change');
    });

    window.addEventListener('load', () => {
      // Inform the initiator page that this page was loaded.
      this.prerenderChannel_.postMessage('loaded');
    });
  }
}

/**
 * Loads the initiator page, and the page will start a prerender.
 */
function loadInitiatorPage() {
  // Used to communicate with the prerendering page.
  const prerenderChannel = new BroadcastChannel('prerender-channel');
  window.addEventListener('unload', () => {
    prerenderChannel.close();
  });

  // We need to wait for load before navigation since the prerendering
  // implementation in Chromium can only activate if the response for the
  // prerendering navigation has already been received and the prerendering
  // document was created.
  const loaded = new Promise(resolve => {
    prerenderChannel.addEventListener('message', e => {
      resolve(e.data);
    }, {once: true});
  });

  const url = new URL(document.URL);
  url.searchParams.append('prerendering', '');
  // Prerender a page that attempts to execute a deferred promise.
  startPrerendering(url.toString());

  // Navigate to the prerendered page after being informed.
  loaded.then(() => {
    // navigate to the prerenderered page.
    window.location = url.toString();
  });
}
