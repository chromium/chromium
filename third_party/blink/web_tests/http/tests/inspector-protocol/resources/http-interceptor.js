// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to allow renderer tests to conveniently intercept network
 * requests and provide HTTP headers and body.
 */
(class HttpInterceptor {
  /**
   * @param {!TestRunner} testRunner Host TestRunner instance.
   * @param {!Proxy} dp DevTools session protocol instance.
   * @param {!Page} page TestRunner.Page instance.
   */
  constructor(testRunner, dp) {
    this.testRunner_ = testRunner;
    this.dp_ = dp;
    this.disableRequestedUrlsLogging = false;
    this.ignoreFavIconRequests = true;
    this.responses_ = new Map();
    this.requestedUrls_ = [];
    this.requestedMethods_ = [];
  }

  /**
   * Initializes the helper returning reference to itself to allow assignment.
   *
   * @return {!object} HttpInterceptor reference.
   */
  async init() {
    await this.dp_.Fetch.enable({patterns: [{urlPattern: '*'}]});

    this.dp_.Fetch.onRequestPaused(event => {
      const method = event.params.request.method;
      this.requestedMethods_.push(method);

      const url =
          event.params.request.url + (event.params.request.urlFragment || '');
      this.requestedUrls_.push(url);

      if (this.ignoreFavIconRequests && url.endsWith('/favicon.ico')) {
        this.dp_.Fetch.failRequest({requestId: event.params.requestId});
        return;
      }

      var response = this.responses_.get(url);
      if (response) {
        if (!this.disableRequestedUrlsLogging) {
          this.testRunner_.log(`requested url: ${url}`);
        }
      } else {
        this.testRunner_.log(`requested url: ${url} is not known`);
      }
      const body = (response && response.body) || '';
      const headers = (response && response.headers) || [];
      this.dp_.Fetch.fulfillRequest({
        requestId: event.params.requestId,
        responseCode: response.responseCode,
        responsePhrase: response.responsePhrase,
        binaryResponseHeaders: btoa(headers.join('\0')),
        body: btoa(body)
      });
    });

    return this;
  }

  /**
   * Prevents requested urls from being logged. This helps to stabilize tests
   * that request urls in arbitrary order. Use hasRequestedUrls(urls) function
   * to check if expected urls has been requested.
   *
   * @param {boolean} value True if requested url logging is disabled, false
   *     otherwise.
   */
  setDisableRequestedUrlsLogging(value) {
    this.disableRequestedUrlsLogging = value;
  }

  /**
   * Ignores fav icon requests. This fixes tests that are failing due to the
   * missing fav icon request response.
   *
   * @param {boolean} value True if fav icon requests are ignored, false
   *     otherwise.
   */
  setIgnoreFavIconRequests(value) {
    this.ignoreFavIconRequests = value;
  }

  /**
   * Adds request response.
   *
   * @param {!string} url Request url, including #fragment.
   * @param {?string} body Request response body, optional.
   * @param {?[string]} headers Request response headers, optional.
   */
  addResponse(url, body, headers) {
    let responseCode = 200;
    let responsePhrase = 'OK'

    if (headers) {
      const statusLine = headers[0];
      const match = statusLine.match(/HTTP\/1.1 (\d{1,3}) *(.*)/);
      if (match) {
        headers.shift();
        responseCode = +match[1];
        responsePhrase = match[2];
      }
    }
    this.responses_.set(url, {body, headers, responseCode, responsePhrase});
  }

  /**
   * Logs requested methods in the order requests have been received.
   */
  logRequestedMethods() {
    this.testRunner_.log(`Requested methods: ${this.requestedMethods_.length}`);
    for (const method of this.requestedMethods_) {
      this.testRunner_.log(` ${method}`);
    }
  }

  /**
   * Returns the array of requested URLs.
   *
   * @return {!Array<string>}
   */
  requestedUrls() {
    return this.requestedUrls_;
  }

  /**
   * Checks if specified urls have been requested.
   *
   * @param {[string]} urls Array of urls to check against requested urls.
   */
  hasRequestedUrls(urls) {
    this.testRunner_.log(`Expected requested urls:`);
    for (const url of urls) {
      if (this.requestedUrls_.indexOf(url) >= 0) {
        this.testRunner_.log(` ${url}`);
      } else {
        this.testRunner_.log(` ${url} is MISSING`);
      }
    }
  }
});
