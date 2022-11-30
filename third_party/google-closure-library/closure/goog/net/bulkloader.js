/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Loads a list of URIs in bulk. All requests must be a success
 * in order for the load to be considered a success.
 */

goog.provide('goog.net.BulkLoader');

goog.require('goog.events.Event');
goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');
goog.require('goog.log');
goog.require('goog.net.BulkLoaderHelper');
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.requireType('goog.Uri');



/**
 * Class used to load multiple URIs.
 * @param {Array<string|goog.Uri>} uris The URIs to load.
 * @constructor
 * @extends {goog.events.EventTarget}
 * @final
 */
goog.net.BulkLoader = function(uris) {
  'use strict';
  goog.events.EventTarget.call(this);

  /**
   * The bulk loader helper.
   * @type {goog.net.BulkLoaderHelper}
   * @private
   */
  this.helper_ = new goog.net.BulkLoaderHelper(uris);

  /**
   * The handler for managing events.
   * @type {goog.events.EventHandler<!goog.net.BulkLoader>}
   * @private
   */
  this.eventHandler_ = new goog.events.EventHandler(this);
};
goog.inherits(goog.net.BulkLoader, goog.events.EventTarget);


/**
 * A logger.
 * @type {goog.log.Logger}
 * @private
 */
goog.net.BulkLoader.prototype.logger_ =
    goog.log.getLogger('goog.net.BulkLoader');


/**
 * Gets the response texts, in order.
 * @return {Array<string>} The response texts.
 */
goog.net.BulkLoader.prototype.getResponseTexts = function() {
  'use strict';
  return this.helper_.getResponseTexts();
};


/**
 * Gets the request Uris.
 * @return {Array<string>} The request URIs, in order.
 */
goog.net.BulkLoader.prototype.getRequestUris = function() {
  'use strict';
  return this.helper_.getUris();
};


/**
 * Starts the process of loading the URIs.
 */
goog.net.BulkLoader.prototype.load = function() {
  'use strict';
  const eventHandler = this.eventHandler_;
  const uris = this.helper_.getUris();
  goog.log.info(
      this.logger_, 'Starting load of code with ' + uris.length + ' uris.');

  for (let i = 0; i < uris.length; i++) {
    const xhrIo = new goog.net.XhrIo();
    eventHandler.listen(
        xhrIo, goog.net.EventType.COMPLETE,
        goog.bind(this.handleEvent_, this, i));

    xhrIo.send(uris[i]);
  }
};


/**
 * Handles all events fired by the XhrManager.
 * @param {number} id The id of the request.
 * @param {goog.events.Event} e The event.
 * @private
 */
goog.net.BulkLoader.prototype.handleEvent_ = function(id, e) {
  'use strict';
  goog.log.info(
      this.logger_,
      'Received event "' + e.type + '" for id ' + id + ' with uri ' +
          this.helper_.getUri(id));
  const xhrIo = /** @type {goog.net.XhrIo} */ (e.target);
  if (xhrIo.isSuccess()) {
    this.handleSuccess_(id, xhrIo);
  } else {
    this.handleError_(id, xhrIo);
  }
};


/**
 * Handles when a request is successful (i.e., completed and response received).
 * Stores thhe responseText and checks if loading is complete.
 * @param {number} id The id of the request.
 * @param {goog.net.XhrIo} xhrIo The XhrIo objects that was used.
 * @private
 */
goog.net.BulkLoader.prototype.handleSuccess_ = function(id, xhrIo) {
  'use strict';
  // Save the response text.
  this.helper_.setResponseText(id, xhrIo.getResponseText());

  // Check if all response texts have been received.
  if (this.helper_.isLoadComplete()) {
    this.finishLoad_();
  }
  xhrIo.dispose();
};


/**
 * Handles when a request has ended in error (i.e., all retries completed and
 * none were successful). Cancels loading of the URI's.
 * @param {number|string} id The id of the request.
 * @param {goog.net.XhrIo} xhrIo The XhrIo objects that was used.
 * @private
 */
goog.net.BulkLoader.prototype.handleError_ = function(id, xhrIo) {
  'use strict';
  // TODO(user): Abort all pending requests.

  // Dispatch the ERROR event.
  this.dispatchEvent(new goog.net.BulkLoader.LoadErrorEvent(xhrIo.getStatus()));
  xhrIo.dispose();
};


/**
 * Finishes the load of the URI's. Dispatches the SUCCESS event.
 * @private
 */
goog.net.BulkLoader.prototype.finishLoad_ = function() {
  'use strict';
  goog.log.info(this.logger_, 'All uris loaded.');

  // Dispatch the SUCCESS event.
  this.dispatchEvent(goog.net.EventType.SUCCESS);
};


/** @override */
goog.net.BulkLoader.prototype.disposeInternal = function() {
  'use strict';
  goog.net.BulkLoader.superClass_.disposeInternal.call(this);

  this.eventHandler_.dispose();
  this.eventHandler_ = null;

  this.helper_.dispose();
  this.helper_ = null;
};


/**
 * @param {number} status The response status.
 * @constructor
 * @extends {goog.events.Event}
 * @final
 * @protected
 */
goog.net.BulkLoader.LoadErrorEvent = function(status) {
  'use strict';
  goog.net.BulkLoader.LoadErrorEvent.base(
      this, 'constructor', goog.net.EventType.ERROR);

  /** @type {number} */
  this.status = status;
};
goog.inherits(goog.net.BulkLoader.LoadErrorEvent, goog.events.Event);
