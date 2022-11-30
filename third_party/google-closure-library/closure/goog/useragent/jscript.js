/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Detection of JScript version.
 */


goog.provide('goog.userAgent.jscript');

goog.require('goog.string');


/**
 * @define {boolean} True if it is known at compile time that the runtime
 *     environment will not be using JScript.
 */
goog.userAgent.jscript.ASSUME_NO_JSCRIPT =
    goog.define('goog.userAgent.jscript.ASSUME_NO_JSCRIPT', false);


/**
 * Whether we detect that the user agent is using Microsoft JScript.
 * @type {boolean}
 */
goog.userAgent.jscript.HAS_JSCRIPT = false;


/**
 * The installed version of JScript.
 * @type {string}
 */
goog.userAgent.jscript.VERSION = '0';


/**
 * Initializer for goog.userAgent.jscript.  Detects if the user agent is using
 * Microsoft JScript and which version of it.
 *
 * This is a named function so that it can be stripped via the jscompiler
 * option for stripping types.
 * @package
 */
goog.userAgent.jscript.init = function() {
  'use strict';
  var hasScriptEngine = 'ScriptEngine' in goog.global;
  goog.userAgent.jscript.HAS_JSCRIPT =
      hasScriptEngine && goog.global['ScriptEngine']() == 'JScript';
  if (goog.userAgent.jscript.HAS_JSCRIPT) {
    goog.userAgent.jscript.VERSION = goog.global['ScriptEngineMajorVersion']() +
        '.' + goog.global['ScriptEngineMinorVersion']() + '.' +
        goog.global['ScriptEngineBuildVersion']();
  }
};

if (!goog.userAgent.jscript.ASSUME_NO_JSCRIPT) {
  goog.userAgent.jscript.init();
}

/**
 * Whether the installed version of JScript is as new or newer than a given
 * version.
 * @param {string} version The version to check.
 * @return {boolean} Whether the installed version of JScript is as new or
 *     newer than the given version.
 */
goog.userAgent.jscript.isVersion = function(version) {
  'use strict';
  return goog.string.compareVersions(goog.userAgent.jscript.VERSION, version) >=
      0;
};
