/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.provide('goog.pubsub.TopicId');



/**
 * A templated class that is used to register `goog.pubsub.PubSub`
 * subscribers.
 *
 * Typical usage for a publisher:
 * <code>
 *   /** @type {!goog.pubsub.TopicId<!zorg.State>}
 *   zorg.TopicId.STATE_CHANGE = new goog.pubsub.TopicId(
 *       goog.events.getUniqueId('state-change'));
 *
 *   // Compiler enforces that these types are correct.
 *   pubSub.publish(zorg.TopicId.STATE_CHANGE, zorg.State.STARTED);
 * </code>
 *
 * Typical usage for a subscriber:
 * <code>
 *   // Compiler enforces the callback parameter type.
 *   pubSub.subscribe(zorg.TopicId.STATE_CHANGE, function(state) {
 *     if (state == zorg.State.STARTED) {
 *       // Handle STARTED state.
 *     }
 *   });
 * </code>
 *
 * @param {string} topicId
 * @template PAYLOAD
 * @constructor
 * @final
 * @struct
 */
goog.pubsub.TopicId = function(topicId) {
  'use strict';
  /**
   * @const
   * @private
   */
  this.topicId_ = topicId;
};


/** @override */
goog.pubsub.TopicId.prototype.toString = function() {
  'use strict';
  return this.topicId_;
};
