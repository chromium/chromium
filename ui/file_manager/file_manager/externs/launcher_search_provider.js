// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for chrome.launcherSearchProvider API. Since this API is under
 * development and idl change may happen, we put extern file under
 * ui/file_manager as temporary.
 */
chrome.launcherSearchProvider = {};

/**
 * @param {number} queryId
 * @param {Array<{itemId:string, title:string, iconUrl:?string,
 *     relevance:number}>} results
 */
chrome.launcherSearchProvider.setSearchResults = function(queryId, results) {};

/**
 * @type {!ChromeEvent}
 */
chrome.launcherSearchProvider.onQueryStarted;

/**
 * @type {!ChromeEvent}
 */
chrome.launcherSearchProvider.onQueryEnded;

/**
 * @type {!ChromeEvent}
 */
chrome.launcherSearchProvider.onOpenResult;
