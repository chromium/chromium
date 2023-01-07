// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(wnwen): Move most of these functions to their own page rather than
//     common, which should be shared with content script.

class Common {
  /**
   * TODO(wnwen): Remove this and use actual web API.
   */
  static $(id) {
    return document.getElementById(id);
  }

  static siteFromUrl(url) {
    return new URL(url).hostname;
  }

  /**
   * The filter should not apply to these URLs.
   * @param {string} url The URL to check.
   */
  static isDisallowedUrl(url) {
    return url.startsWith('chrome') || url.startsWith('about');
  }

  /**
   * Whether extension is loaded unpacked or from Chrome Webstore.
   * @const {boolean}
   */
  static IS_DEV_MODE = !('update_url' in chrome.runtime.getManifest());

  /**
   * Easily turn on/off console logs.
   * @param {*} logArgs The message to potentially pass to {@code console.log}.
   */
  static debugPrint(logArgs) {
    if (Common.IS_DEV_MODE)
      console.log.apply(console, arguments);
  }
}
