/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Announcer that allows messages to be spoken by assistive
 * technologies.
 */

goog.provide('goog.a11y.aria.Announcer');
goog.require('goog.Disposable');
// TODO(user): Fix issue breaking external projects and remove goog.Timer
// dependency.
/** @suppress {extraRequire} */
goog.require('goog.Timer');
goog.require('goog.a11y.aria');
goog.require('goog.a11y.aria.LivePriority');
goog.require('goog.a11y.aria.State');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.object');
goog.require('goog.string');



/**
 * Class that allows messages to be spoken by assistive technologies that the
 * user may have active.
 *
 * @param {goog.dom.DomHelper=} opt_domHelper DOM helper.
 * @constructor
 * @extends {goog.Disposable}
 * @final
 */
goog.a11y.aria.Announcer = function(opt_domHelper) {
  'use strict';
  goog.a11y.aria.Announcer.base(this, 'constructor');

  /**
   * @type {goog.dom.DomHelper}
   * @private
   */
  this.domHelper_ = opt_domHelper || goog.dom.getDomHelper();

  /**
   * Map of priority to live region elements to use for communicating updates.
   * Elements are created on demand.
   * @type {Object<goog.a11y.aria.LivePriority, !Element>}
   * @private
   */
  this.liveRegions_ = {};
  /** @private {string} */
  this.lastMessageAnnounced_ = '';
};
goog.inherits(goog.a11y.aria.Announcer, goog.Disposable);


/** @override */
goog.a11y.aria.Announcer.prototype.disposeInternal = function() {
  'use strict';
  goog.object.forEach(
      this.liveRegions_, this.domHelper_.removeNode, this.domHelper_);
  this.liveRegions_ = null;
  this.domHelper_ = null;
  goog.a11y.aria.Announcer.base(this, 'disposeInternal');
};


/**
 * Announce a message to be read by any assistive technologies the user may
 * have active.
 * @param {string} message The message to announce to screen readers.
 * @param {goog.a11y.aria.LivePriority=} opt_priority The priority of the
 *     message. Defaults to POLITE.
 */
goog.a11y.aria.Announcer.prototype.say = function(message, opt_priority) {
  'use strict';
  const priority = opt_priority || goog.a11y.aria.LivePriority.POLITE;
  const liveRegion = this.getLiveRegion_(priority);
  // TODO(user): Remove the code once Chrome fix the bug on their
  // end. Add nonbreaking space such that there's a change to aria live region
  // to verbalize repeated character or text.
  const announceMessage = this.lastMessageAnnounced_ === message ?
      message + goog.string.Unicode.NBSP :
      message;
  if (message) {
    this.lastMessageAnnounced_ = announceMessage;
  }
  goog.dom.setTextContent(liveRegion, announceMessage);
};

/**
 * Returns the id value for an aria-live region for a given priority.
 * @param {!goog.a11y.aria.LivePriority} priority The required priority.
 * @return {string} The generated id on the liveRegion.
 */
goog.a11y.aria.Announcer.prototype.getLiveRegionId = function(priority) {
  return this.getLiveRegion_(priority).getAttribute('id');
};

/**
 * Returns an aria-live region that can be used to communicate announcements.
 * @param {!goog.a11y.aria.LivePriority} priority The required priority.
 * @return {!Element} A live region of the requested priority.
 * @private
 */
goog.a11y.aria.Announcer.prototype.getLiveRegion_ = function(priority) {
  'use strict';
  var liveRegion = this.liveRegions_[priority];
  if (liveRegion) {
    // Make sure the live region is not aria-hidden.
    goog.a11y.aria.removeState(liveRegion, goog.a11y.aria.State.HIDDEN);
    return liveRegion;
  }

  liveRegion = this.domHelper_.createElement(goog.dom.TagName.DIV);
  // Generate a unique id for the live region.
  liveRegion.id = `goog-lr-${goog.getUid(liveRegion)}`;
  // Note that IE has a habit of declaring things that aren't display:none as
  // invisible to third-party tools like JAWs, so we can't just use height:0.
  liveRegion.style.position = 'absolute';
  liveRegion.style.top = '-1000px';
  liveRegion.style.height = '1px';
  liveRegion.style.overflow = 'hidden';
  goog.a11y.aria.setState(liveRegion, goog.a11y.aria.State.LIVE, priority);
  goog.a11y.aria.setState(liveRegion, goog.a11y.aria.State.ATOMIC, 'true');
  this.domHelper_.getDocument().body.appendChild(liveRegion);
  this.liveRegions_[priority] = liveRegion;
  return liveRegion;
};
