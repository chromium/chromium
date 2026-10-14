// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared helper for the signed exchange tests. It records the Network and Log
// events for navigations that load a signed exchange and prints the requests
// together with the Network.signedExchangeReceived info.
(class SXGHelper {
  constructor(testRunner, session) {
    this._testRunner = testRunner;
    this._session = session;
    this._logEntries = [];
    // One entry per request, in the order they were started. A redirect
    // (including the synthesized redirect to the inner URL of a signed
    // exchange) starts a new entry with the same requestId.
    this._requests = [];
    this._currentRequestById = new Map();
    this._finishedCallbacks = [];
  }

  async enable() {
    const dp = this._session.protocol;
    this._listenNetwork(dp);
    dp.Log.onEntryAdded(({params}) => this._logEntries.push(params.entry));
    // The inner response of a signed exchange loaded in a cross-site iframe
    // commits in an out-of-process iframe, which reports its own
    // Network.loadingFinished event.
    dp.Target.onAttachedToTarget(async ({params}) => {
      const child = this._session.createChild(params.sessionId);
      child.protocol.Network.onLoadingFinished(
          event => this._onLoadingFinished(event));
      await child.protocol.Network.enable();
      await child.protocol.Runtime.runIfWaitingForDebugger();
    });
    await dp.Target.setAutoAttach(
        {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
    await dp.Network.enable();
    await dp.Log.enable();
  }

  _listenNetwork(dp) {
    dp.Network.onRequestWillBeSent(event => this._onRequestWillBeSent(event));
    dp.Network.onResponseReceived(({params}) => {
      const request = this._currentRequestById.get(params.requestId);
      if (request) {
        request.statusCode = params.response.status;
      }
    });
    dp.Network.onSignedExchangeReceived(({params}) => {
      const request = this._currentRequestById.get(params.requestId);
      if (!request) {
        return;
      }
      request.signedExchangeInfo = params.info;
      request.resourceType = 'signed-exchange';
      request.statusCode = params.info.outerResponse.status;
    });
    dp.Network.onLoadingFinished(event => this._onLoadingFinished(event));
    dp.Network.onLoadingFailed(({params}) => {
      const request = this._currentRequestById.get(params.requestId);
      if (request) {
        request.failed = true;
        this._onFinished(request);
      }
    });
  }

  _onRequestWillBeSent({params}) {
    const previous = this._currentRequestById.get(params.requestId);
    if (previous && params.redirectResponse && !previous.signedExchangeInfo) {
      previous.statusCode = params.redirectResponse.status;
    }
    const request = {
      url: params.request.url,
      failed: false,
      statusCode: 0,
      resourceType: (params.type || 'Other').toLowerCase(),
      signedExchangeInfo: null,
      transferSize: 0,
      redirectSource: previous || null,
    };
    this._requests.push(request);
    this._currentRequestById.set(params.requestId, request);
  }

  _onLoadingFinished({params}) {
    const request = this._currentRequestById.get(params.requestId);
    if (!request) {
      return;
    }
    // Like the DevTools frontend, attribute the bytes transferred for the
    // inner response to the signed exchange.
    if (request.redirectSource?.signedExchangeInfo) {
      request.redirectSource.transferSize = params.encodedDataLength;
    } else {
      request.transferSize = params.encodedDataLength;
    }
    this._onFinished(request);
  }

  _onFinished(request) {
    const callbacks = this._finishedCallbacks;
    this._finishedCallbacks = [];
    for (const callback of callbacks) {
      if (!callback(request)) {
        this._finishedCallbacks.push(callback);
      }
    }
  }

  // Resolves once a request matching `urlRegex` has finished or failed.
  waitForRequestFinished(urlRegex) {
    return new Promise(resolve => {
      this._finishedCallbacks.push(request => {
        if (!urlRegex.test(request.url)) {
          return false;
        }
        resolve(request);
        return true;
      });
    });
  }

  // Adds an iframe to the page and waits for its load event.
  addIframe(url) {
    function addIframeInPage(url) {
      return new Promise(resolve => {
        const iframe = document.createElement('iframe');
        iframe.src = url;
        iframe.onload = () => resolve();
        document.body.appendChild(iframe);
      });
    }
    return this._session.evaluateAsync(addIframeInPage, url);
  }

  requests() {
    return this._requests;
  }

  dumpLogEntries() {
    for (const entry of this._logEntries) {
      this._testRunner.log(`Log entry (${entry.level}): ${entry.text}`);
    }
  }

  dumpRequestsWithSignedExchangeInfo() {
    const log = this._testRunner.log.bind(this._testRunner);
    for (const request of this._requests) {
      log(`* ${request.url}`);
      log(`  failed: ${request.failed}`);
      log(`  statusCode: ${request.statusCode}`);
      log(`  resourceType: ${request.resourceType}`);
      const info = request.signedExchangeInfo;
      if (!info) {
        continue;
      }
      log('  SignedExchangeInfo');
      if (info.header) {
        log(`    Request URL: ${info.header.requestUrl}`);
        for (const signature of info.header.signatures) {
          log(`    Certificate URL: ${signature.certUrl}`);
        }
      }
      if (info.securityDetails) {
        log(`    Certificate Subject: ${info.securityDetails.subjectName}`);
        log(`    Certificate Issuer: ${info.securityDetails.issuer}`);
      }
      for (const error of info.errors || []) {
        log(`    Error: ${JSON.stringify(error)}`);
      }
    }
  }
})
