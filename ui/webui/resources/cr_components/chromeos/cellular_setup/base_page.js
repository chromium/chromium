// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Base template with elements common to all Cellular Setup flow sub-pages. */
Polymer({
  is: 'base-page',

  properties: {
    /**
     * Main title for the page.
     *
     * @type {string}
     */
    title: String,

    /**
     * Message displayed under the main title.
     *
     * @type {string}
     */
    message: String,
  },
});
