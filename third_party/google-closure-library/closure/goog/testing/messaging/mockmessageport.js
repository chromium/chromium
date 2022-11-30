/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A simple dummy class for representing message ports in tests.
 */

goog.setTestOnly('goog.testing.messaging.MockMessagePort');
goog.provide('goog.testing.messaging.MockMessagePort');

goog.require('goog.events.EventTarget');
goog.require('goog.testing.MockControl');



/**
 * Class for unit-testing code that uses MessagePorts.
 * @param {*} id An opaque identifier, used because message ports otherwise have
 *     no distinguishing characteristics.
 * @param {goog.testing.MockControl} mockControl The mock control used to create
 *     the method mock for #postMessage.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @final
 */
goog.testing.messaging.MockMessagePort = function(id, mockControl) {
  'use strict';
  goog.testing.messaging.MockMessagePort.base(this, 'constructor');

  /**
   * An opaque identifier, used because message ports otherwise have no
   * distinguishing characteristics.
   * @type {*}
   */
  this.id = id;

  /**
   * Whether or not the port has been started.
   * @type {boolean}
   */
  this.started = false;

  /**
   * Whether or not the port has been closed.
   * @type {boolean}
   */
  this.closed = false;

  mockControl.createMethodMock(this, 'postMessage');
};
goog.inherits(goog.testing.messaging.MockMessagePort, goog.events.EventTarget);


/**
 * A mock postMessage funciton. Actually an instance of
 * {@link goog.testing.FunctionMock}.
 * @param {*} message The message to send.
 * @param {Array<MessagePort>=} opt_ports Ports to send with the message.
 */
goog.testing.messaging.MockMessagePort.prototype.postMessage = function(
    message, opt_ports) {};


/**
 * Starts the port.
 */
goog.testing.messaging.MockMessagePort.prototype.start = function() {
  'use strict';
  this.started = true;
};


/**
 * Closes the port.
 */
goog.testing.messaging.MockMessagePort.prototype.close = function() {
  'use strict';
  this.closed = true;
};
