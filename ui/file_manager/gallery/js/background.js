// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Configuration of the Gallery window.
 * @type {!Object}
 * @const
 */
const windowCreateOptions = {
  id: 'gallery',
  outerBounds: {
    minWidth: 860,
    minHeight: 554,
  },
  frame: {
    color: '#1E2023',
  },
  hidden: true
};

/**
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {!BackgroundBase}
 */
window.background = new BackgroundBaseImpl();

/**
 * Gallery app window wrapper.
 * @type {!SingletonAppWindowWrapper}
 */
const galleryWrapper =
    new SingletonAppWindowWrapper('gallery.html', windowCreateOptions);

/**
 * Opens gallery window.
 * @param {!Array<string>} urls List of URL to show.
 * @return {!Promise} Promise to be fulfilled on success, or rejected on error.
 */
function openGalleryWindow(urls) {
  return new Promise(function(fulfill, reject) {
           util.URLsToEntries(urls)
               .then(function(result) {
                 fulfill(util.entriesToURLs(result.entries));
               })
               .catch(reject);
         })
      .then(function(urls) {
        if (urls.length === 0) {
          return Promise.reject('No file to open.');
        }

        // Opens a window.
        return galleryWrapper.launch({urls: urls}, false)
            .then(function() {
              const galleryWrapperDocument =
                  galleryWrapper.rawAppWindow.contentWindow.document;
              if (galleryWrapperDocument.readyState == 'complete') {
                return galleryWrapper;
              }

              return new Promise(function(fulfill, reject) {
                galleryWrapperDocument.addEventListener(
                    'DOMContentLoaded', fulfill.bind(null, galleryWrapper));
              });
            });
      })
      .then(function(galleryWrapper) {
        // If the window is minimized, we need to restore it first.
        if (galleryWrapper.rawAppWindow.isMinimized()) {
          galleryWrapper.rawAppWindow.restore();
        }

        galleryWrapper.rawAppWindow.show();

        return galleryWrapper.rawAppWindow.contentWindow.appID;
      })
      .catch(function(error) {
        console.error('Launch failed: ' + (error.stack || error));
        return Promise.reject(error);
      });
}

window.background.setLaunchHandler(openGalleryWindow);
