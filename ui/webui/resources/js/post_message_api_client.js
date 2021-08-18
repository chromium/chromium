// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that provides the functionality for talking to a PostMessageAPIServer
 * over the postMessage API.  This should be subclassed and the methods in the
 * server that the client needs to access should be provided in methodList.
 *
 */
export class PostMessageAPIClient {
  /**
   * @param {!Array<string>} methodList The list of methods accessible via the
   *     client.
   * @param {!string} serverOriginURLFilter  Only messages from this origin
   *     will be accepted.
   */
  constructor(methodList, serverOriginURLFilter) {
    /**
     * @private @const {!string} Filter to use to validate
     * the origin of received messages.  The origin of messages
     * must exactly match this value.
     */
    this.serverOriginURLFilter_ = serverOriginURLFilter;

    /**
     * The parent window.
     * @private {Window}
     */
    this.parentWindow_ = null;
    /*
     * @private {number}
     */
    this.nextMethodId_ = 0;
    /**
     * Map of methods awaiting a response.
     * @private {!Map}
     */
    this.methodsAwaitingResponse_ = new Map;
    /**
     * Function property that tracks whether client has
     * been initialized by the server.
     * @private {Function}
     */
    this.boundOnInitialize_ = this.onInitialize_.bind(this);

    // Wait for an init message from the server.
    window.addEventListener('message', this.boundOnInitialize_);
  }

  /**
   * Virtual method called when the client is initialized and it knows the
   * server that it is communicating with. This method should be overwritten by
   * subclasses which would like to know when initialization is done.
   */
  onInitialized() {}

  //
  // Private implementation:
  //

  /**
   * Handles initialization event sent from the server to establish
   * communication.
   * @private
   * @param {!Event} event  An event received when the initialization message is
   *     sent from the server.
   */
  onInitialize_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.error(
          'Initialization event received from non-authorized origin: ' +
          event.origin);
      return;
    }
    this.parentWindow_ = event.source;
    this.parentWindow_.postMessage('init', this.serverOriginURLFilter_);
    window.removeEventListener('message', this.boundOnInitialize_);
    this.boundOnInitialize_ = null;
    window.addEventListener('message', this.onMessage_.bind(this));
    this.onInitialized();
  }

  /**
   * Determine if the specified server origin URL matches the origin filter.
   * @param {!string} origin The origin URL to match with the filter.
   * @return {boolean}  whether the specified origin matches the filter.
   */
  originMatchesFilter(origin) {
    return (new URL(origin)).toString() === this.serverOriginURLFilter_;
  }

  /**
   * Handles postMessage events sent from the server.
   * @param {Event} event  An event received from the server via the postMessage
   *     API.
   */
  onMessage_(event) {
    if (!this.originMatchesFilter(event.origin)) {
      console.error(
          'Message received from non-authorized origin: ' + event.origin);
      return;
    }
    if (event.source !== this.parentWindow_) {
      console.error('discarding event whose source is not the parent window');
      return;
    }
    if (!this.methodsAwaitingResponse_.has(event.data.methodId)) {
      if (event.data === 'init') {
        console.log('Received init message after initialization is complete.');
        this.parentWindow_.postMessage('init', this.serverOriginURLFilter_);
        return;
      } else {
        console.error('discarding event method is not waiting for a response');
        return;
      }
    }
    const method = this.methodsAwaitingResponse_.get(event.data.methodId);
    this.methodsAwaitingResponse_.delete(event.data.methodId);
    method(event.data.result);
  }

  /**
   * Converts a function call with arguments into a postMessage event
   * and sends it to the server via the postMessage API.
   * @param {string} fn  The function to call.
   * @param {!Array<Object>} args The arguments to pass to the function.
   * @return {!Promise} A promise capturing the executing of the function.
   */
  callApiFn(fn, args) {
    const newMethodId = this.nextMethodId_++;
    const promise = new Promise((resolve, reject) => {
      if (!this.parentWindow_) {
        reject('No parent window defined');
      }
      this.parentWindow_.postMessage(
          {
            methodId: newMethodId,
            fn: fn,
            args: args,
          },
          this.serverOriginURLFilter_);

      this.methodsAwaitingResponse_.set(newMethodId, resolve);
    });
    return promise;
  }
}
