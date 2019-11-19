// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge to MathJax functions from the ChromeVox content script.
 *
 */

if (typeof(goog) != 'undefined' && goog.provide) {
  goog.provide('cvox.MathJax');
}

if (typeof(goog) != 'undefined' && goog.require) {
  goog.require('cvox.Api');
  goog.require('cvox.MathJaxExternalUtil');
}

(function() {
  /**
   * The channel between the page and content script.
   * @type {MessageChannel}
   */
  var channel_ = new MessageChannel();


  /**
   * @constructor
   */
  cvox.MathJax = function() {
  };


  /**
   * Initializes message channel in Chromevox.
   */
  cvox.MathJax.initMessage = function() {
    channel_.port1.onmessage = function(evt) {
      cvox.MathJax.execMessage(evt.data);
    };
    window.postMessage('cvox.MathJaxPortSetup', '*', [channel_.port2]);
  };


  /**
   * Post a message to Chromevox.
   * @param {string} cmd The command to be executed in Chromevox.
   * @param {string} callbackId A string representing the callback id.
   * @param {Object<string, *>} args Dictionary of arguments.
   */
  cvox.MathJax.postMessage = function(cmd, callbackId, args) {
    channel_.port1.postMessage({'cmd': cmd, 'id': callbackId, 'args': args});
  };


  /**
   * Executes a command for an incoming message.
   * @param {{cmd: string, id: string, args: string}} msg A
   *     serializable message.
   */
  cvox.MathJax.execMessage = function(msg) {
    var args = msg.args;
    switch (msg.cmd) {
      case 'Active':
        cvox.MathJax.isActive(msg.id);
      break;
      case 'AllJax':
        cvox.MathJax.getAllJax(msg.id);
      break;
      case 'AsciiMathToMml':
        cvox.MathJax.asciiMathToMml(msg.id, args.alt, args.id);
      break;
      case 'InjectScripts':
        cvox.MathJax.injectScripts();
      break;
      case 'ConfWikipedia':
        cvox.MathJax.configMediaWiki();
      break;
      case 'RegSig':
        cvox.MathJax.registerSignal(msg.id, args.sig);
      break;
      case 'TexToMml':
        cvox.MathJax.texToMml(msg.id, args.alt, args.id);
      break;
    }
  };


  /**
   * Compute the MathML representation for all currently available MathJax
   * nodes.
   * @param {string} callbackId A string representing the callback id.
   */
  cvox.MathJax.getAllJax = function(callbackId) {
    cvox.MathJaxExternalUtil.getAllJax(
        cvox.MathJax.getMathmlCallback_(callbackId));
  };


  /**
   * Registers a callback for a particular Mathjax signal.
   * @param {string} callbackId A string representing the callback id.
   * @param {string} signal The Mathjax signal on which to fire the callback.
   */
  cvox.MathJax.registerSignal = function(callbackId, signal) {
    cvox.MathJaxExternalUtil.registerSignal(
        cvox.MathJax.getMathmlCallback_(callbackId), signal);
  };


  /**
   * Constructs a callback that posts a string with the MathML representation of
   * a MathJax element to ChromeVox.
   * @param {string} callbackId A string representing the callback id.
   * @return {function(Node, string)} A function taking a Mathml node and an id
   * string.
   * @private
   */
  cvox.MathJax.getMathmlCallback_ = function(callbackId) {
    return function(mml, id) {
      cvox.MathJax.postMessage('NodeMml', callbackId,
                               {'mathml': mml, 'elementId': id});
    };
  };


  /**
   * Inject a minimalistic MathJax script into a page for LaTeX translation.
   */
  cvox.MathJax.injectScripts = function() {
    cvox.MathJaxExternalUtil.injectConfigScript();
    cvox.MathJaxExternalUtil.injectLoadScript();
  };


  /**
   * Loads configurations for MediaWiki pages (e.g., Wikipedia).
   */
  cvox.MathJax.configMediaWiki = function() {
        cvox.MathJaxExternalUtil.configMediaWiki();
  };


  /**
   * Translates a LaTeX expressions into a MathML element.
   * @param {string} callbackId A string representing the callback id.
   * @param {string} tex The LaTeX expression.
   * @param {string} cvoxId A string representing the cvox id for the node.
   */
  cvox.MathJax.texToMml = function(callbackId, tex, cvoxId) {
    cvox.MathJaxExternalUtil.texToMml(
        function(mml) {
          cvox.MathJax.getMathmlCallback_(callbackId)(mml, cvoxId);
        },
        tex);
  };


  /**
   * Translates an AsciiMath expression into a MathML element.
   * @param {string} callbackId A string representing the callback id.
   * @param {string} asciiMath The AsciiMath expression.
   * @param {string} cvoxId A string representing the cvox id for the node.
   */
  cvox.MathJax.asciiMathToMml = function(callbackId, asciiMath, cvoxId) {
    cvox.MathJaxExternalUtil.asciiMathToMml(
        function(mml) {
          cvox.MathJax.getMathmlCallback_(callbackId)(mml, cvoxId);
        },
        asciiMath);
  };


  /**
   * Check if MathJax is injected in the page.
   * @param {string} callbackId A string representing the callback id.
   */
  cvox.MathJax.isActive = function(callbackId) {
    cvox.MathJax.postMessage(
        'Active', callbackId,
        {'status': cvox.MathJaxExternalUtil.isActive()});
  };


  // Initializing the bridge.
  cvox.MathJax.initMessage();

})();
