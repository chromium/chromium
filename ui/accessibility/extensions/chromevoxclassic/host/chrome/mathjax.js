// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implementation of ChromeVox's bridge to MathJax.
 *
 */

goog.provide('cvox.ChromeMathJax');

goog.require('cvox.AbstractMathJax');
goog.require('cvox.ApiImplementation');
goog.require('cvox.ChromeVox');
goog.require('cvox.HostFactory');
goog.require('cvox.ScriptInstaller');


/**
 * @constructor
 * @extends {cvox.AbstractMathJax}
 */
cvox.ChromeMathJax = function() {
  goog.base(this);

  /**
   * Set to when the bridge is initialized.
   * @type {boolean}
   * @private
   */
  this.initialized_ = false;

  /**
   * The port to communicate with the content script.
   * @type {Port}
   */
  this.port = null;

  /**
   * The next id to use for async callbacks.
   * @type {number}
   * @private
   */
  this.nextCallbackId_ = 1;

  /**
   * Map from callback ID to callback function.
   * @type {Object<number, Function>}
   * @private
   */
  this.callbackMap_ = {};

  /**
   * The ids for converted TeX nodes.
   * @type {number}
   * @private
   */
  this.texNodeId_ = 0;

  this.init();
};
goog.inherits(cvox.ChromeMathJax, cvox.AbstractMathJax);


/**
 * Register a callback function in the mapping.
 * @param {Function} callback The callback function.
 * @return {number} id The new id.
 * @private
 */
cvox.ChromeMathJax.prototype.registerCallback_ = function(callback) {
  var id = this.nextCallbackId_;
  this.nextCallbackId_++;
  this.callbackMap_[id] = callback;
  return id;
};


/**
 * Destructive Retrieval of a callback function from the mapping.
 * @param {string} idStr The id.
 * @return {Function} The callback function.
 * @private
 */
cvox.ChromeMathJax.prototype.retrieveCallback_ = function(idStr) {
  var id = parseInt(idStr, 10);
  var callback = this.callbackMap_[id];
  if (callback) {
    return callback;
  }
  return null;
};


/**
 * Initialise communication with the content script.
 */
cvox.ChromeMathJax.prototype.init = function() {
  window.addEventListener('message', goog.bind(this.portSetup, this), true);
  var scripts = new Array();
  scripts.push(cvox.ChromeVox.host.getFileSrc(
      'chromevox/injected/mathjax_external_util.js'));
  scripts.push(cvox.ChromeVox.host.getFileSrc('chromevox/injected/mathjax.js'));
  scripts.push(cvox.ApiImplementation.siteSpecificScriptLoader);
  this.initialized_ = cvox.ScriptInstaller.installScript(
      scripts, 'mathjax', undefined,
      cvox.ApiImplementation.siteSpecificScriptBase);
};


/**
 * Destructive Retrieval of a callback function from the mapping.
 * @param {string} data The command to be sent to the content script.
 * @param {Function} callback A callback function.
 * @param {Object<*>=} args Object of arguments.
 */
cvox.ChromeMathJax.prototype.postMsg = function(data, callback, args) {
  args = args || {};
  var id = this.registerCallback_(callback);
  var idStr = id.toString();
  this.port.postMessage({'cmd': data, 'id': idStr, 'args': args});
};


/**
 * This method is called when the content script receives a message from
 * the page.
 * @param {Event} event The DOM event with the message data.
 * @return {boolean} True if default event processing should continue.
 */
cvox.ChromeMathJax.prototype.portSetup = function(event) {
  if (event.data == 'cvox.MathJaxPortSetup') {
    this.port = event.ports[0];
    this.port.onmessage =
        goog.bind(
            function(event) {this.dispatchMessage(event.data);},
            this);
    return false;
  }
  return true;
};


/**
 * Call the appropriate Cvox function dealing with MathJax return values.
 * @param {{cmd: string, id: string, args: Object<string>}} message A
 * message object.
 */
cvox.ChromeMathJax.prototype.dispatchMessage = function(message) {
  var method;
  var argNames = [];
  switch (message['cmd']) {
    case 'NodeMml':
      method = this.convertMarkupToDom;
      argNames = ['mathml', 'elementId'];
    break;
    case 'Active':
      method = this.applyBoolean;
      argNames = ['status'];
    break;
  }

  if (!method) {
    throw 'Unknown MathJax call: ' + message['cmd'];
  }
  var callback = this.retrieveCallback_(message['id']);
  var args = message['args'];
  if (callback && method) {
    method.apply(this,
                 [callback].concat(
                     argNames.map(function(x) {return args[x];})));
  }
};


/**
 * Converts a Boolean string to boolean value and applies a callback function.
 * @param {function(boolean)} callback A function with one argument.
 * @param {boolean} bool A truth value.
  */
cvox.ChromeMathJax.prototype.applyBoolean = function(
    callback, bool) {
  callback(bool);
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.isMathjaxActive = function(callback) {
  if (!this.initialized_) {
    callback(false);
    return;
  }

  var retries = 0;

  var fetch = goog.bind(function() {
    retries++;
    try {this.postMsg('Active',
                      function(result) {
                        if (result) {
                          callback(result);
                        } else if (retries < 5) {
                          setTimeout(fetch, 1000);
                        }
                      });
        } catch (x) {  // Error usually means that the port is not ready yet.
          if (retries < 5) {
            setTimeout(fetch, 1000);
          } else {
            throw x;
          }}},
                        this);

  fetch();
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.getAllJax = function(callback) {
  this.postMsg('AllJax', callback);
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.registerSignal = function(
    callback, signal) {
  this.postMsg('RegSig', callback, {sig: signal});
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.injectScripts = function() {
  var retries = 0;

  var fetch = goog.bind(
      function() {
        retries++;
        if (this.port) {
          this.postMsg('InjectScripts', function() {});
        } else if (retries < 10) {
          setTimeout(fetch, 500);
        }
      },
      this);

  fetch();
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.configMediaWiki = function() {
  this.postMsg('ConfWikipedia', function() { });
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.getTex = function(callback, tex) {
  var altText = tex['alt'] || tex['title'];
  if (altText) {
    var newId = 'cvoxId-' + this.texNodeId_++;
    tex.setAttribute('cvoxId', newId);
    this.postMsg('TexToMml', callback, {alt: altText, id: newId});
  }
};


/**
 * @override
 */
cvox.ChromeMathJax.prototype.getAsciiMath = function(callback, asciiMathNode) {
  var altText = asciiMathNode['alt'] || asciiMathNode['title'];
  if (altText) {
    var newId = 'cvoxId-' + this.texNodeId_++;
    asciiMathNode.setAttribute('cvoxId', newId);
    this.postMsg('AsciiMathToMml', callback, {alt: altText, id: newId});
  }
};


/** Export platform constructor. */
cvox.HostFactory.mathJaxConstructor =
    cvox.ChromeMathJax;
