importScripts('sw-test-helpers.js');

var pendingPopupResultResolver = null;

function waitForPopupResult() {
  return new Promise(resolve => {
    pendingPopupResultResolver = resolve;
  });
}

var TESTS = [
  async function testPopupDelegation() {
    // 1. Synthesize click to get transient activation in SW.
    await synthesizeNotificationClick();

    // 2. Post message to client window with delegation.
    let resultPromise = waitForPopupResult();
    client.postMessage({type: 'delegate-popup'}, {delegate: 'popup'});

    let result = await resultPromise;
    self.postMessage('popup delegation result: ' + result);
    runNextTestOrQuit();
  },

  async function testPopupDelegationWithoutActivation() {
    // Post message without synthesizing click first (no activation in SW).
    try {
      client.postMessage({type: 'delegate-popup'}, {delegate: 'popup'});
      self.postMessage('postMessage should have thrown');
    } catch (e) {
      self.postMessage('postMessage threw: ' + e.name);
    }
    runNextTestOrQuit();
  },

  async function testDoublePostMessageDelegation() {
    // 1. Synthesize click to get transient activation in SW.
    await synthesizeNotificationClick();

    // 2. First postMessage with delegation consumes the SW token.
    let resultPromise = waitForPopupResult();
    client.postMessage({type: 'delegate-popup'}, {delegate: 'popup'});

    // 3. Second postMessage with delegation in the same click handler must
    // throw NotAllowedError.
    try {
      client.postMessage({type: 'delegate-popup'}, {delegate: 'popup'});
      self.postMessage('second postMessage should have thrown');
    } catch (e) {
      self.postMessage('second postMessage threw: ' + e.name);
    }

    await resultPromise;
    runNextTestOrQuit();
  },

  async function testUnsupportedCapabilityDelegation() {
    await synthesizeNotificationClick();
    try {
      client.postMessage({type: 'delegate-popup'}, {delegate: 'fullscreen'});
      self.postMessage('unsupported capability postMessage should have thrown');
    } catch (e) {
      self.postMessage('unsupported capability postMessage threw: ' + e.name);
    }
    runNextTestOrQuit();
  },

  async function testPopupDelegationDoubleOpen() {
    // 1. Synthesize click to get transient activation in SW.
    await synthesizeNotificationClick();

    // 2. Post message to client window with delegation.
    let resultPromise = waitForPopupResult();
    client.postMessage({type: 'delegate-popup', test: 'double-open'},
                       {delegate: 'popup'});

    let result = await resultPromise;
    self.postMessage('popup delegation double open result: ' + result);
    runNextTestOrQuit();
  },

  async function testPopupDelegationExpired() {
    // 1. Synthesize click to get transient activation in SW.
    await synthesizeNotificationClick();

    // 2. Post message to client window with delegation.
    let resultPromise = waitForPopupResult();
    client.postMessage({type: 'delegate-popup', test: 'expired'},
                       {delegate: 'popup'});

    let result = await resultPromise;
    self.postMessage('popup delegation expired result: ' + result);
    runNextTestOrQuit();
  },

  async function testFocusAndPopupDelegation() {
    // 1. Synthesize click to get transient activation in SW.
    const e = await synthesizeNotificationClick();

    // 2. Focus the client window first (using AllowWindowInteraction).
    // Extend event lifetime across async focus() call using e.waitUntil.
    let resolveTest;
    e.waitUntil(new Promise(resolve => {
      resolveTest = resolve;
    }));

    await client.focus();

    // 3. Post message to client window with delegation.
    let resultPromise = waitForPopupResult();
    client.postMessage({type: 'delegate-popup'}, {delegate: 'popup'});

    let result = await resultPromise;
    self.postMessage('focus and popup delegation result: ' + result);
    resolveTest();
    runNextTestOrQuit();
  },

  async function testNonWindowClientDelegation() {
    await synthesizeNotificationClick();
    let workerClients = [];
    for (let i = 0; i < 10; ++i) {
      workerClients = await self.clients.matchAll({type: 'worker'});
      if (workerClients.length > 0)
        break;
      await new Promise(r => setTimeout(r, 50));
    }
    if (workerClients.length > 0) {
      try {
        workerClients[0].postMessage({type: 'delegate-popup'},
                                     {delegate: 'popup'});
        self.postMessage('worker postMessage should have thrown');
      } catch (e) {
        self.postMessage('worker postMessage threw: ' + e.name);
      }
    } else {
      self.postMessage('no worker clients found');
    }
    runNextTestOrQuit();
  }
];

self.onmessage = function(e) {
  if (e.data == 'start') {
    e.waitUntil(initialize().then(runNextTestOrQuit));
  } else if (e.data && e.data.type === 'popup-result') {
    if (pendingPopupResultResolver) {
      var resolve = pendingPopupResultResolver;
      pendingPopupResultResolver = null;
      resolve(e.data.result);
    }
  } else {
    e.waitUntil(initialize().then(function() {
      self.postMessage('received unexpected message: ' +
                       JSON.stringify(e.data));
      self.postMessage('quit');
    }));
  }
};
