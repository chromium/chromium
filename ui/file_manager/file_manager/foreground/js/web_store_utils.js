// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {uselessCode} Temporary suppress because of the line exporting.
 */

// #import {constants} from './constants.m.js';

/**
 * Namespace for web store utility functions.
 * @namespace
 */
const webStoreUtils = {};

/**
 * Location of the Chrome Web Store.
 *
 * @const
 * @type {string}
 */
webStoreUtils.CHROME_WEB_STORE_URL = 'https://chrome.google.com/webstore';

/**
 * Base URL of apps list in the Chrome Web Store.
 *
 * @const
 * @type {string}
 */
webStoreUtils.WEB_STORE_HANDLER_BASE_URL =
    'https://chrome.google.com/webstore/category/collection/file_handlers';

/**
 * Returns URL of the Chrome Web Store which show apps supporting the given
 * file-extension and mime-type.
 *
 * @param {?string} extension Extension of the file (with the first dot).
 * @param {?string} mimeType Mime type of the file.
 * @return {string} URL
 */
webStoreUtils.createWebStoreLink = (extension, mimeType) => {
  if (!extension || constants.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1) {
    return webStoreUtils.CHROME_WEB_STORE_URL;
  }

  if (extension[0] === '.') {
    extension = extension.substr(1);
  } else {
    console.warn('Please pass an extension with a dot to createWebStoreLink.');
  }

  let url = webStoreUtils.WEB_STORE_HANDLER_BASE_URL;
  url += '?_fe=' + extension.toLowerCase().replace(/[^\w]/g, '');

  // If a mime is given, add it into the URL.
  if (mimeType) {
    url += '&_fmt=' + mimeType.replace(/[^-\w\/]/g, '');
  }
  return url;
};

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {webStoreUtils};
