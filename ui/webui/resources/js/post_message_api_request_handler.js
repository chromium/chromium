// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handler for requests that may come to the PostMessageAPIClient. This enables
// the client to support duplex communication.
export class RequestHandler {
  constructor() {
    /**
     * Map that stores references to the methods implemented by the API.
     * @private {!Map<string, function(!Array):?>}
     */
    this.apiFns_ = new Map();
  }

  /**
   * Registers the specified method name with the specified
   * function.
   *
   * @param {!string} methodName name of the method to register.
   * @param {!function(!Array):?} method The function to associate with the
   *     name.
   */
  registerMethod(methodName, method) {
    this.apiFns_.set(methodName, method);
  }

  /**
   * Executes the method and returns the result.
   *
   * @param {!string} funcName name of method to be executed.
   * @param {!Array} args the arguments to the method being executed.
   * @return {Promise<!Object>} returns a promise of the object returned.
   */
  handle(funcName, args) {
    if (!this.apiFns_.has(funcName)) {
      return Promise.reject('Unknown function requested' + funcName);
    }

    return this.apiFns_.get(funcName)(args);
  }

  /**
   * Check whether the method can be handled by this handler.
   * @param{!string} funcName name of method
   * @return {boolean}
   */
  canHandle(funcName) {
    return this.apiFns_.has(funcName);
  }
}
