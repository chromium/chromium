// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for global |chrome| object. These methods are defined
 * in WebUIExtension::Install.
 * @externs
 */

/**
 * @param {string} msg
 * @param {Array=} opt_args
 */
chrome.send = function(msg, opt_args) {};

/**
 * @param {string} name The name of the variable set  with SetWebUIProperty()
 * @return {string} JSON variable value
 */
chrome.getVariableValue = function(name) {};
