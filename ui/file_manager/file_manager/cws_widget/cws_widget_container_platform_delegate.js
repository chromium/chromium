// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Strings required by the widget container.
 * @typedef {{
 *   UI_LOCALE: string,
 *   LINK_TO_WEBSTORE: string,
 *   INSTALLATION_FAILED_MESSAGE: string,
 *   LOADING_SPINNER_ALT: string,
 *   INSTALLING_SPINNER_ALT: string
 * }}
 */
let CWSWidgetContainerStrings;

/**
 * Functions for reporting metrics for the widget.
 * @typedef {{
 *   recordEnum: function(string, number, number),
 *   recordUserAction: function(string),
 *   startInterval: function(string),
 *   recordInterval: function(string)
 * }}
 */
let CWSWidgetContainerMetricsImpl;

/**
 * Type for delegate used by CWSWidgetContainer component to access Chrome
 * platform APIs.
 * @typedef {{
 *   strings: !CWSWidgetContainerStrings,
 *   metricsImpl: !CWSWidgetContainerMetricsImpl,
 *   installWebstoreItem: function(string, function(?string)),
 *   getInstalledItems: function(function(?Array<!string>)),
 *   requestWebstoreAccessToken: function(function(?string))
 * }}
 */
let CWSWidgetContainerPlatformDelegate;
