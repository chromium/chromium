// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileOverview This file loads the MOCK cast APIs for test.
 */

chrome.cast = {};

/**
 * @enum {string}
 * @const
 */
chrome.cast.ReceiverAvailability = {
  UNAVAILABLE: 'unavailable',
  AVAILABLE: 'available',
};
Object.freeze(chrome.cast.ReceiverAvailability);

/**
 * @constructor
 */
chrome.cast.SessionRequest = function() {
};

/**
 * @constructor
 * @param {chrome.cast.SessionRequest} sessionRequest
 * @param {function(chrome.cast.Session)} onSession
 * @param {function(chrome.cast.ReceiverAvailability)} onReceiver
 */
chrome.cast.ApiConfig = function(sessionRequest, onSession, onReceiver) {
  this.onReceiver_ = onReceiver;

  Object.seal(this);
};

/**
 * @param {chrome.cast.ApiConfig} apiConfig
 * @param {function()} onInitSuccess
 * @param {function(chrome.cast.Error)} onError
 */
chrome.cast.initialize = function(apiConfig, onInitSuccess, onError) {
  this.apiConfig_ = apiConfig;

  const receiver1 = {friendlyName: 'test cast', label: 'testcast'};
  const receivers = [receiver1];
  setTimeout(this.apiConfig_.onReceiver_.bind(
      null, chrome.cast.ReceiverAvailability.UNAVAILABLE, []));
  setTimeout(this.apiConfig_.onReceiver_.bind(
      null, chrome.cast.ReceiverAvailability.AVAILABLE, receivers), 1000);

  onInitSuccess();
};

/**
 * Initialized apiConfig value.
 * @type {chrome.cast.ApiConfig}
 * @private
 */
chrome.cast.apiConfig_ = null;

/**
 * @const
 */
chrome.cast.isAvailable = true;

Object.seal(chrome.cast);

// Invokes the handler.
if (window['__onGCastApiAvailable']) {
  window['__onGCastApiAvailable'](true, null);
}
