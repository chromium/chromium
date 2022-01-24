/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

// Use of this source code is governed by the Apache License, Version 2.0.
// See the COPYING file for details.

/**
 * @fileoverview A web worker for integration testing the PortChannel class.
 *
 * @nocompile
 */

self.CLOSURE_BASE_PATH =
self.CLOSURE_BASE_PATH = '../../';
importScripts('../../bootstrap/webworkers.js');
importScripts('../../base.js');

// The provide is necessary to stop the jscompiler from thinking this is an
// entry point and adding it into the manifest incorrectly.
goog.provide('goog.messaging.testdata.portchannel_worker');
goog.require('goog.messaging.PortChannel');

function registerPing(channel) {
  channel.registerService('ping', function(msg) {
    'use strict';
    channel.send('pong', msg);
  }, true);
}

function startListening() {
  const channel = new goog.messaging.PortChannel(self);
  registerPing(channel);

  channel.registerService('addPort', function(port) {
    'use strict';
    port.start();
    registerPing(new goog.messaging.PortChannel(port));
  }, true);
}

startListening();
// Signal to portchannel_test that the worker is ready.
postMessage('loaded');
