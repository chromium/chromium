// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview A host factory.  This factory allows us to decouple the
 * cvox.Host|Tts|... creatation from the main ChromeVox code.
 */

goog.provide('cvox.HostFactory');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.AbstractHost');
goog.require('cvox.AbstractMathJax');
goog.require('cvox.AbstractTts');


/**
 * @constructor
 */
cvox.HostFactory = function() {};

/**
 * Returns the host.
 * @return {cvox.AbstractHost}
 */
cvox.HostFactory.getHost = function() {
  return new cvox.HostFactory.hostConstructor;
};

/**
 * Returns the TTS interface.
 * @return {cvox.TtsInterface} The TTS engine.
 */
cvox.HostFactory.getTts = function() {
  return new cvox.HostFactory.ttsConstructor;
};

/**
 * Returns the Braille interface.
 * @return {cvox.BrailleInterface} The Braille interface.
 */
cvox.HostFactory.getBraille = function() {
  return new cvox.HostFactory.brailleConstructor;
};

/**
 * Returns the earcons interface.
 * @return {cvox.AbstractEarcons}
 */
cvox.HostFactory.getEarcons = function() {
  return new cvox.HostFactory.earconsConstructor;
};

/**
 * Returns the MathJax interface.
 * @return {cvox.MathJaxInterface} The MathJax interface.
 */
cvox.HostFactory.getMathJax = function() {
  return new cvox.HostFactory.mathJaxConstructor;
};

/**
 * @type {function (new:cvox.AbstractHost)}
 */
cvox.HostFactory.hostConstructor;

/**
 * @type {function (new:cvox.TtsInterface)}
 */
cvox.HostFactory.ttsConstructor;

/**
 * @type {function (new:cvox.BrailleInterface)}
 */
cvox.HostFactory.brailleConstructor;

/**
 * @type {function (new:cvox.AbstractEarcons)}
 */
cvox.HostFactory.earconsConstructor;


/**
 * @type {function (new:cvox.AbstractMathJax)}
 */
cvox.HostFactory.mathJaxConstructor;
