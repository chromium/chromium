// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {resolveIsolatedEntries} from '../../common/js/api.js';
import {util} from '../../common/js/util.js';
import {BackgroundBase, LaunchHandler} from '../../externs/background/background_base.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * Root class of the background page.
 * @implements {BackgroundBase}
 */
export class BackgroundBaseImpl {
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
        if (!loadTimeData.isInitialized()) {
          loadTimeData.data = assert(stringData);
        }
        fulfill(stringData);
      });
    });

    /** @private {?LaunchHandler} */
    this.launchHandler_ = null;

    // Initialize handlers.
    if (!window.isSWA) {
      chrome.app.runtime.onLaunched.addListener(this.onLaunched_.bind(this));
      chrome.app.runtime.onRestarted.addListener(this.onRestarted_.bind(this));
    }
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
   *     chrome.app.runtime.onLaunched for detail.
   */
  async onLaunched_(launchData) {
    // Skip if files are not selected.
    if (!launchData || !launchData.items || launchData.items.length == 0) {
      return;
    }

    await this.initializationPromise_;

    // Volume list needs to be initialized (more precisely,
    // chrome.fileManagerPrivate.getVolumeRoot needs to be called to grant
    // access).
    await volumeManagerFactory.getInstance();

    const isolatedEntries = launchData.items.map(item => item.entry);

    let urls = [];
    try {
      // Obtains entries in non-isolated file systems.
      // The entries in launchData are stored in the isolated file system.
      // We need to map the isolated entries to the normal entries to retrieve
      // their parent directory.
      const externalEntries =
          await retryResolveIsolatedEntries(isolatedEntries);
      urls = util.entriesToURLs(externalEntries);
    } catch (error) {
      // Just log the error and default no file/URL so we spawn the app window.
      console.warn(error);
      urls = [];
    }

    if (this.launchHandler_) {
      this.launchHandler_(urls);
    }
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

/** @private {number} Total number of retries for the resolve entries below.*/
const MAX_RETRIES = 6;

/**
 * Retry the resolveIsolatedEntries() until we get the same number of entries
 * back.
 * @param {!Array<!Entry>} isolatedEntries Entries that need to be resolved.
 * @return {!Promise<!Array<!Entry>>} Promise resolved with the entries
 *   resolved.
 */
async function retryResolveIsolatedEntries(isolatedEntries) {
  let count = 0;
  let externalEntries = [];
  // Wait time in milliseconds between attempts. We double this value after
  // every wait.
  let waitTime = 25;

  // Total waiting time is ~1.5 second for `waitTime` starting at 25ms and total
  // of 6 attempts.
  while (count <= MAX_RETRIES) {
    externalEntries = await resolveIsolatedEntries(isolatedEntries);
    if (externalEntries.length >= isolatedEntries.length) {
      return externalEntries;
    }

    console.warn(`Failed to resolve, retrying in ${waitTime}ms...`);
    await new Promise(resolve => setTimeout(resolve, waitTime));
    waitTime = waitTime * 2;
    count += 1;
  }

  console.warn(
      `Failed to resolve: Requested ${isolatedEntries.length},` +
      ` resolved: ${externalEntries.length}.`);
  return [];
}
