// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the ScriptInstaller functions which install scripts
 * into the web page (not a content script)
 *
 */

goog.provide('cvox.ScriptInstaller');


/**
 * URL pattern where we do not allow script installation.
 * @type {RegExp}
 */
cvox.ScriptInstaller.blacklistPattern = /chrome:\/\/|chrome-extension:\/\//;

/**
 * Installs a script in the web page.
 * @param {Array<string>} srcs An array of URLs of scripts.
 * @param {string} uid A unique id.  This function won't install the same set of
 *      scripts twice.
 * @param {function()=} opt_onload A function called when the last script
 *     has loaded.
 * @param {string=} opt_chromevoxScriptBase An optional chromevoxScriptBase
 *     attribute to add.
 * @return {boolean} False if the script already existed and this function
 * didn't do anything.
 */
cvox.ScriptInstaller.installScript = function(
    srcs, uid, opt_onload, opt_chromevoxScriptBase) {
  if (cvox.ScriptInstaller.blacklistPattern.test(document.URL)) {
    return false;
  }
  if (document.querySelector('script[' + uid + ']')) {
    cvox.ScriptInstaller.uninstallScript(uid);
  }
  if (!srcs || srcs.length == 0) {
    return false;
  }

  cvox.ScriptInstaller.installScriptHelper_(
      srcs, uid, opt_onload, opt_chromevoxScriptBase);
  return true;
};

/**
 * Uninstalls a script.
 * @param {string} uid Id of the script node.
 */
cvox.ScriptInstaller.uninstallScript = function(uid) {
  var scriptNode;
  if (scriptNode = document.querySelector('script[' + uid + ']'))
    scriptNode.remove();
};

/**
 * Helper that installs one script and calls itself recursively when each
 * script loads.
 * @param {Array<string>} srcs An array of URLs of scripts.
 * @param {string} uid A unique id.  This function won't install the same set of
 *      scripts twice.
 * @param {function()=} opt_onload A function called when the
 *     last script has loaded.
 * @param {string=} opt_chromevoxScriptBase An optional chromevoxScriptBase
 *     attribute to add.
 * @private
 */
cvox.ScriptInstaller.installScriptHelper_ = function(
    srcs, uid, opt_onload, opt_chromevoxScriptBase) {
  function next() {
    if (srcs.length > 0) {
      cvox.ScriptInstaller.installScriptHelper_(
          srcs, uid, opt_onload, opt_chromevoxScriptBase);
    } else if (opt_onload) {
      opt_onload();
    }
  }

  var scriptSrc = srcs.shift();
  if (!scriptSrc) {
    next();
    return;
  }

  var xhr = new XMLHttpRequest();
  var url = scriptSrc + '?' + new Date().getTime();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == 4) {
      var scriptText = xhr.responseText;
      var apiScript = document.createElement('script');
      apiScript.type = 'text/javascript';
      apiScript.setAttribute(uid, '1');
      apiScript.textContent = scriptText;
      if (opt_chromevoxScriptBase) {
        apiScript.setAttribute('chromevoxScriptBase', opt_chromevoxScriptBase);
      }
      var scriptOwner = document.head || document.body;
      scriptOwner.appendChild(apiScript);
      next();
    }
  };

  try {
    xhr.open('GET', url, true);
    xhr.send(null);
  } catch (exception) {
    console.log(
        'Warning: ChromeVox external script loading for ' + document.location +
        ' stopped after failing to install ' + scriptSrc);
    next();
  }
};
