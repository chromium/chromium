// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This is a type to let closure compiler recognize extended methods to Window
 * instance at gallery/js/gallery.js.
 */
class GalleryWindow extends Window {
  constructor() {
    /**
     * @type {Promise}
     */
    this.initializePromise;
  }
}
