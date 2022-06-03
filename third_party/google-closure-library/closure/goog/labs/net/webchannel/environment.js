/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A single module to define user-agent specific environment
 * details.
 *
 */

goog.module('goog.labs.net.webChannel.environment');

goog.module.declareLegacyNamespace();

var userAgent = goog.require('goog.userAgent');


/**
 * The default polling interval in millis for Edge.
 *
 * Currently on edge, new-chunk events may be not be fired (at all) if a new
 * chunk arrives within 50ms following the previous chunk. This may be fixed
 * in future, which requires changes to the whatwg spec too.
 *
 * @private @const {number}
 */
var EDGE_POLLING_INTERVAL_ = 125;


/**
 * History:
 *
 * IE11 is still using Trident, the traditional engine for IE.
 * Edge is using EdgeHTML, a fork of Trident. We are seeing the same issue
 * on IE-11 (reported in 2017), so treat IE the same as Edge for now.
 *
 * We used to do polling for Opera (only) with an 250ms interval, because Opera
 * only fires readyState == INTERACTIVE once. Opera switched to WebKit in 2013,
 * and then to Blink (chrome).
 *
 * TODO(user): check the raw UA string to keep polling for old, mobile operas
 * that may still be affected. For old Opera, double the polling interval
 * to 250ms.
 *
 * @return {boolean} True if polling is required with XHR.
 */
exports.isPollingRequired = function() {
  return userAgent.EDGE_OR_IE;
};


/**
 * How often to poll (in MS) for changes to responseText in browsers that don't
 * fire onreadystatechange during incremental loading of the response body.
 *
 * @return {number|undefined} The polling interval (MS) for the current U-A;
 * or undefined if polling is not supposed to be enabled.
 */
exports.getPollingInterval = function() {
  if (userAgent.EDGE_OR_IE) {
    return EDGE_POLLING_INTERVAL_;
  }

  return undefined;
};

/**
 * Origin trial token for google.com
 *
 * https://developers.chrome.com/origintrials/#/trials
 *
 * http://googlechrome.github.io/OriginTrials/check-token.html
 * Origin: https://google.com:443
 * Matches Subdomains? Yes
 * Matches Third-party? Yes
 * Feature: FetchUploadStreaming
 * Expires: 7/14/2021, 8:59:59 AM
 *
 * Token for googleapis.com will be registered after google.com's is deployed.
 *
 */
const OT_TOKEN_GOOGLE_COM =
    "A70X6iKIlnS3U/OFBWYlZCJ6rRlXum75MZ6pvi68FKsnyeL+XPCA7KWBMeW75d2+xNHMEeFOWjfqMS+34jdvrw4AAAB/eyJvcmlnaW4iOiJodHRwczovL2dvb2dsZS5jb206NDQzIiwiZmVhdHVyZSI6IkZldGNoVXBsb2FkU3RyZWFtaW5nIiwiZXhwaXJ5IjoxNjI2MjIwNzk5LCJpc1N1YmRvbWFpbiI6dHJ1ZSwiaXNUaGlyZFBhcnR5Ijp0cnVlfQ==";
/**
 * Creates ReadableStream to upload
 * @return {!ReadableStream} ReadableStream to upload
 */
function createStream() {
  const encoder = new goog.global.TextEncoder();
  return new goog.global.ReadableStream({
    start: controller => {
      for (const obj of ['test\r\n', 'test\r\n']) {
        controller.enqueue(encoder.encode(obj));
      }
      controller.close();
    }
  });
}

/**
 * Detect the user agent is chrome and its version is higher than M90.
 * This code is hard-coded from goog.labs.userAgent.browser to avoid file size
 * increasing.
 * @return {boolean} Whether the above is true.
 */
function isChromeM90OrHigher() {
  const userAgentStr = function() {
    const navigator = goog.global.navigator;
    if (navigator) {
      const userAgent = navigator.userAgent;
      if (userAgent) {
        return userAgent;
      }
    }
    return '';
  }();

  const matchUserAgent = function (str) {
    return userAgentStr.indexOf(str) != -1;
  };

  if (!matchUserAgent('Chrome') || matchUserAgent('Edg')) {
    return false;
  }

  const match = /Chrome\/(\d+)/.exec(userAgentStr);
  const chromeVersion = parseInt(match[1], 10);
  return chromeVersion >= 90;
}

/**
 * Detect the URL origin is *.google.com.
 * @param {string} url The target URL.
 * @return {boolean} Whether the above is true.
 */
function isUrlGoogle(url) {
  const match = /\/\/([^\/]+)\//.exec(url);
  if (!match) {
    return false;
  }
  const origin = match[1];
  return origin.endsWith("google.com");
}

/**
 * The flag to run the origin trials code only once.
 */
let isStartOriginTrialsCalled = false;

/**
 * For Fetch/upload OT, make three requests against the server endpoint.
 * POST requests contain only dummy payload.
 *
 * https://developers.chrome.com/origintrials/#/view_trial/3524066708417413121
 *
 * This function is expected to be called from background during the handshake.
 * Exceptions will be logged by the caller.
 *
 * No stats or logs are collected on the client-side. To be disabled once the
 * OT is expired.
 *
 * @param {string} path The base URL path for the requests
 * @param {function(*)} logError A function to execute when exceptions are
 *     caught.
 */
exports.startOriginTrials = function(path, logError) {
  if (isStartOriginTrialsCalled) {
    return;
  }
  isStartOriginTrialsCalled = true;
  // NE: may need check if path has already contains query params?

  // Accept only Chrome M90 or later due to service worker support.
  if (!isChromeM90OrHigher()) {
    return;
  }

  // Accept only only google.com and subdoamins.
  if(!isUrlGoogle(path)) {
    return;
  }
  // Since 3P OT is not supported yet, we should check the current page matches
  // the path (absolute one?) to disable this OT for cross-origin calls
  if(!window || !window.document || !isUrlGoogle(window.document.URL)) {
    return;
  }

  // Enable origin trial by injecting OT <meta> tag
  const tokenElement =
    /** @type {! HTMLMetaElement} */ (document.createElement('meta'));
  tokenElement.httpEquiv = 'origin-trial';
  tokenElement.content = OT_TOKEN_GOOGLE_COM;
  // appendChild() synchronously enables OT.
  document.head.appendChild(tokenElement);

  // Check if fetch upload stream is actually enabled.
  // By the spec, Streaming request doesn't has the Content-Type header:
  // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
  // If Chrome doesn't support Streaming, the body stream is converted to a
  // string "[object ReadableStream]" for fallback then it has "Content-Type:
  // text/plain;charset=UTF-8".
  const supportsRequestStreams = !new Request('', {
    body: new ReadableStream(),
    method: 'POST',
  }).headers.has('Content-Type');
  if (!supportsRequestStreams) {
    return;
  }

  // 1st req:  path?ot=1
  // non-streaming upload request
  goog.global.fetch(`${path}?ot=1`, {method: 'POST', body: 'test\r\n'})
    .catch(logError);

  // 2nd req:  path?ot=2
  // h2-only streaming upload request
  goog.global.fetch(`${path}?ot=2`, {
    method: 'POST',
    body: createStream(),
    allowHTTP1ForStreamingUpload: false,
  }).catch(logError);

  // 3rd req:  path?ot=3
  // h1-allowed streaming upload request
  goog.global.fetch(`${path}?ot=3`, {
    method: 'POST',
    body: createStream(),
    allowHTTP1ForStreamingUpload: true,
  }).catch(logError);

  // Example calling a Chrome API:
  // goog.global.chrome.loadTimes().wasFetchedViaSpdy
};
