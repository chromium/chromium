/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A fake PortNetwork implementation that simply produces
 * MockMessageChannels for all ports.
 */

goog.setTestOnly('goog.testing.messaging.MockPortNetwork');
goog.provide('goog.testing.messaging.MockPortNetwork');

goog.require('goog.messaging.PortNetwork');
// interface
goog.require('goog.testing.messaging.MockMessageChannel');
goog.requireType('goog.testing.MockControl');



/**
 * The fake PortNetwork.
 *
 * @param {!goog.testing.MockControl} mockControl The mock control for creating
 *     the mock message channels.
 * @constructor
 * @implements {goog.messaging.PortNetwork}
 * @final
 */
goog.testing.messaging.MockPortNetwork = function(mockControl) {
  'use strict';
  /**
   * The mock control for creating mock message channels.
   * @type {!goog.testing.MockControl}
   * @private
   */
  this.mockControl_ = mockControl;

  /**
   * The mock ports that have been created.
   * @type {!Object<!goog.testing.messaging.MockMessageChannel>}
   * @private
   */
  this.ports_ = {};
};


/**
 * Get the mock port with the given name.
 * @param {string} name The name of the port to get.
 * @return {!goog.testing.messaging.MockMessageChannel} The mock port.
 * @override
 */
goog.testing.messaging.MockPortNetwork.prototype.dial = function(name) {
  'use strict';
  if (!(name in this.ports_)) {
    this.ports_[name] =
        new goog.testing.messaging.MockMessageChannel(this.mockControl_);
  }
  return this.ports_[name];
};
