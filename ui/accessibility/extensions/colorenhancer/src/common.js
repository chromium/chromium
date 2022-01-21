// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Move most of these functions to their own page rather than
//     common, which should be shared with content script.

/**
 * TODO(wnwen): Remove this and use actual web API.
 */
function $(id) {
  return document.getElementById(id);
}


/**
 * TODO(wnwen): Remove this, it's terrible.
 */
function siteFromUrl(url) {
  return new URL(url).hostname;
}


/**
 * The filter should not apply to these URLs.
 *
 * @param {string} url The URL to check.
 */
function isDisallowedUrl(url) {
  return url.startsWith('chrome') || url.startsWith('about');
}


/**
 * Whether extension is loaded unpacked or from Chrome Webstore.
 * @const {boolean}
 */
var IS_DEV_MODE = !('update_url' in chrome.runtime.getManifest());


/**
 * Easily turn on/off console logs.
 *
 * @param {*} message The message to potentially pass to {@code console.log}.
 */
function debugPrint(str) {
  if (IS_DEV_MODE)
    console.log(str);
}
