/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Emoji implementation.
 */

goog.provide('goog.ui.emoji.Emoji');



/**
 * Creates an emoji.
 *
 * A simple wrapper for an emoji.
 *
 * @param {string} url URL pointing to the source image for the emoji.
 * @param {string} id The id of the emoji, e.g., 'std.1'.
 * @param {number=} opt_height The height of the emoji, if undefined the
 *     natural height of the emoji is used.
 * @param {number=} opt_width The width of the emoji, if undefined the natural
 *     width of the emoji is used.
 * @param {string=} opt_altText The alt text for the emoji image, eg. the
 *     unicode character representation of the emoji.
 * @constructor
 * @final
 */
goog.ui.emoji.Emoji = function(url, id, opt_height, opt_width, opt_altText) {
  'use strict';
  /**
   * The URL pointing to the source image for the emoji
   *
   * @type {string}
   * @private
   */
  this.url_ = url;

  /**
   * The id of the emoji
   *
   * @type {string}
   * @private
   */
  this.id_ = id;

  /**
   * The height of the emoji
   *
   * @type {?number}
   * @private
   */
  this.height_ = opt_height || null;

  /**
   * The width of the emoji
   *
   * @type {?number}
   * @private
   */
  this.width_ = opt_width || null;

  /**
   * The unicode of the emoji
   *
   * @type {?string}
   * @private
   */
  this.altText_ = opt_altText || null;
};


/**
 * The name of the goomoji attribute, used for emoji image elements.
 * @type {string}
 * @deprecated Use goog.ui.emoji.Emoji.DATA_ATTRIBUTE instead.
 */
goog.ui.emoji.Emoji.ATTRIBUTE = 'goomoji';


/**
 * The name of the goomoji data-attribute, used for emoji image elements. Data
 * attributes are the preferred way in HTML5 to set custom attributes.
 * @type {string}
 */
goog.ui.emoji.Emoji.DATA_ATTRIBUTE = 'data-' + goog.ui.emoji.Emoji.ATTRIBUTE;


/**
 * @return {string} The URL for this emoji.
 */
goog.ui.emoji.Emoji.prototype.getUrl = function() {
  'use strict';
  return this.url_;
};


/**
 * @return {string} The id of this emoji.
 */
goog.ui.emoji.Emoji.prototype.getId = function() {
  'use strict';
  return this.id_;
};


/**
 * @return {?number} The height of this emoji.
 */
goog.ui.emoji.Emoji.prototype.getHeight = function() {
  'use strict';
  return this.height_;
};


/**
 * @return {?number} The width of this emoji.
 */
goog.ui.emoji.Emoji.prototype.getWidth = function() {
  'use strict';
  return this.width_;
};


/**
 * @return {?string} The alt text for the emoji image, eg. the unicode character
 *     representation of the emoji.
 */
goog.ui.emoji.Emoji.prototype.getAltText = function() {
  'use strict';
  return this.altText_;
};
