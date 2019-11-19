// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Defines the ChromeVox app.
 */

window.CLOSURE_USE_EXT_MESSAGES = true;

goog.require('Msgs');
goog.require('cvox.ChromeBraille');
goog.require('cvox.ChromeEarcons');
goog.require('cvox.ChromeHost');
goog.require('cvox.ChromeMathJax');
goog.require('cvox.ChromeTts');
goog.require('cvox.ChromeVoxInit');

if (COMPILED) {
  // NOTE(deboer): This is called when this script is loaded, automatically
  // starting ChromeVox. If this isn't the compiled script, it will be
  // called in init_document.js.
  cvox.ChromeVox.initDocument();
}
