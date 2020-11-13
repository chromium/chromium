// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {BackgroundBase, LaunchHandler} from '../../../externs/background/background_base.m.js';
// #import {VolumeManager} from '../../../externs/volume_manager.m.js';
// #import * as wrappedVolumeManagerFactory from './volume_manager_factory.m.js'; const {volumeManagerFactory} = wrappedVolumeManagerFactory;
// #import * as wrappedUtil from '../../common/js/util.m.js'; const {util} = wrappedUtil;
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// clang-format on

/**
 * Root class of the background page.
 * @implements {BackgroundBase}
 */
/* #export */ class BackgroundBaseImpl {
  constructor() {
    /**
     * Map of all currently open file dialogs. The key is an app ID.
     * @type {!Object<!Window>}
     */
    this.dialogs = {};

    // Initializes the strings. This needs for the volume manager.
    this.initializationPromise_ = new Promise((fulfill, reject) => {
      chrome.fileManagerPrivate.getStrings(stringData => {
        if (chrome.runtime.lastError) {
          console.error(chrome.runtime.lastError.message);
          return;
        }
        loadTimeData.data = assert(stringData);
        fulfill(stringData);
      });
    });

    /** @private {?LaunchHandler} */
    this.launchHandler_ = null;

    // Initialize handlers.
    chrome.app.runtime.onLaunched.addListener(this.onLaunched_.bind(this));
    chrome.app.runtime.onRestarted.addListener(this.onRestarted_.bind(this));
  }

  /**
   * @return {!Promise<!VolumeManager>}
   */
  async getVolumeManager() {
    return volumeManagerFactory.getInstance();
  }

  /**
   * Called when an app is launched.
   *
   * @param {!Object} launchData Launch data. See the manual of
   *     chrome.app.runtime .onLaunched for detail.
   */
  onLaunched_(launchData) {
    // Skip if files are not selected.
    if (!launchData || !launchData.items || launchData.items.length == 0) {
      return;
    }

    this.initializationPromise_
        .then(() => {
          // Volume list needs to be initialized (more precisely,
          // chrome.fileSystem.requestFileSystem needs to be called to grant
          // access) before resolveIsolatedEntries().
          return volumeManagerFactory.getInstance();
        })
        .then(() => {
          const isolatedEntries = launchData.items.map(item => {
            return item.entry;
          });

          // Obtains entries in non-isolated file systems.
          // The entries in launchData are stored in the isolated file system.
          // We need to map the isolated entries to the normal entries to
          // retrieve their parent directory.
          chrome.fileManagerPrivate.resolveIsolatedEntries(
              isolatedEntries, externalEntries => {
                const urls = util.entriesToURLs(externalEntries);
                if (this.launchHandler_) {
                  this.launchHandler_(urls);
                }
              });
        });
  }

  /**
   * Set a handler which is called when an app is launched.
   * @param {!LaunchHandler} handler Function to be called.
   */
  setLaunchHandler(handler) {
    this.launchHandler_ = handler;
  }

  /**
   * Called when an app is restarted.
   */
  onRestarted_() {}
}
