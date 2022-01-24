/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Simple logger that logs to the window console if available.
 *
 * Has an autoInstall option which can be put into initialization code, which
 * will start logging if "Debug=true" is in document.location.href
 */

goog.provide('goog.debug.Console');

goog.require('goog.debug.TextFormatter');
goog.require('goog.log');
goog.requireType('goog.log.LogRecord');


/**
 * Create and install a log handler that logs to window.console if available
 * @constructor
 */
goog.debug.Console = function() {
  'use strict';
  this.publishHandler_ = goog.bind(this.addLogRecord, this);

  /**
   * Formatter for formatted output.
   * @type {!goog.debug.TextFormatter}
   * @private
   */
  this.formatter_ = new goog.debug.TextFormatter();
  this.formatter_.showAbsoluteTime = false;
  this.formatter_.showExceptionText = false;
  // The console logging methods automatically append a newline.
  this.formatter_.appendNewline = false;

  this.isCapturing_ = false;
  this.logBuffer_ = '';

  /**
   * Loggers that we shouldn't output.
   * @type {!Object<boolean>}
   * @private
   */
  this.filteredLoggers_ = {};
};


/**
 * Returns the text formatter used by this console
 * @return {!goog.debug.TextFormatter} The text formatter.
 */
goog.debug.Console.prototype.getFormatter = function() {
  'use strict';
  return this.formatter_;
};


/**
 * Sets whether we are currently capturing logger output.
 * @param {boolean} capturing Whether to capture logger output.
 */
goog.debug.Console.prototype.setCapturing = function(capturing) {
  'use strict';
  if (capturing == this.isCapturing_) {
    return;
  }

  // attach or detach handler from the root logger
  var rootLogger = goog.log.getRootLogger();
  if (capturing) {
    goog.log.addHandler(rootLogger, this.publishHandler_);
  } else {
    goog.log.removeHandler(rootLogger, this.publishHandler_);
  }
  this.isCapturing_ = capturing;
};


/**
 * Adds a log record.
 * @param {?goog.log.LogRecord} logRecord The log entry.
 */
goog.debug.Console.prototype.addLogRecord = function(logRecord) {
  'use strict';
  // Check to see if the log record is filtered or not.
  if (this.filteredLoggers_[logRecord.getLoggerName()]) {
    return;
  }

  /**
   * @param {?goog.log.Level} level
   * @return {string}
   */
  function getConsoleMethodName_(level) {
    if (level) {
      if (level.value >= goog.log.Level.SEVERE.value) {
        // SEVERE == 1000, SHOUT == 1200
        return 'error';
      }
      if (level.value >= goog.log.Level.WARNING.value) {
        return 'warn';
      }
      // NOTE(martone): there's a goog.log.Level.INFO - that we should
      // presumably map to console.info. However, the current mapping is INFO ->
      // console.log. Let's keep the status quo for now, but we should
      // reevaluate if we tweak the goog.log API.
      if (level.value >= goog.log.Level.CONFIG.value) {
        return 'log';
      }
    }
    return 'debug';
  }

  var record = this.formatter_.formatRecord(logRecord);
  var console = goog.debug.Console.console_;
  if (console) {
    // TODO(user): Make getLevel() non-null and update
    // getConsoleMethodName_ parameters.
    var logMethod = getConsoleMethodName_(logRecord.getLevel());
    goog.debug.Console.logToConsole_(
        console, logMethod, record, logRecord.getException());
  } else {
    this.logBuffer_ += record;
  }
};


/**
 * Adds a logger name to be filtered.
 * @param {string} loggerName the logger name to add.
 */
goog.debug.Console.prototype.addFilter = function(loggerName) {
  'use strict';
  this.filteredLoggers_[loggerName] = true;
};


/**
 * Removes a logger name to be filtered.
 * @param {string} loggerName the logger name to remove.
 */
goog.debug.Console.prototype.removeFilter = function(loggerName) {
  'use strict';
  delete this.filteredLoggers_[loggerName];
};


/**
 * Global console logger instance
 * @type {?goog.debug.Console}
 */
goog.debug.Console.instance = null;


/**
 * The console to which to log.  This is a property so it can be mocked out in
 * this unit test for goog.debug.Console. Using goog.global, as console might be
 * used in window-less contexts.
 * @type {{log:!Function}}
 * @private
 */
goog.debug.Console.console_ = goog.global['console'];


/**
 * Sets the console to which to log.
 * @param {!Object} console The console to which to log.
 */
goog.debug.Console.setConsole = function(console) {
  'use strict';
  goog.debug.Console.console_ = /** @type {{log:!Function}} */ (console);
};


/**
 * Install the console and start capturing if "Debug=true" is in the page URL
 */
goog.debug.Console.autoInstall = function() {
  'use strict';
  if (!goog.debug.Console.instance) {
    goog.debug.Console.instance = new goog.debug.Console();
  }

  if (goog.global.location &&
      goog.global.location.href.indexOf('Debug=true') != -1) {
    goog.debug.Console.instance.setCapturing(true);
  }
};


/**
 * Show an alert with all of the captured debug information.
 * Information is only captured if console is not available
 */
goog.debug.Console.show = function() {
  'use strict';
  alert(goog.debug.Console.instance.logBuffer_);
};


/**
 * Logs the record to the console using the given function.  If the function is
 * not available on the console object, the log function is used instead.
 * @param {{log:!Function}} console The console object.
 * @param {string} fnName The name of the function to use.
 * @param {string} record The record to log.
 * @param {*} exception An additional exception to log.
 * @private
 */
goog.debug.Console.logToConsole_ = function(
    console, fnName, record, exception) {
  'use strict';
  if (console[fnName]) {
    console[fnName](record, exception === undefined ? '' : exception);
  } else {
    console.log(record, exception === undefined ? '' : exception);
  }
};
