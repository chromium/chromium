// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {AndroidAppListModel} from './android_app_list_model.js';

/**
 * @param {!Array<string>} appNames List app names to be added.
 * @returns {!AndroidAppListModel} fake for unittests.
 */
export function createFakeAndroidAppListModel(appNames) {
  /**
   * AndroidAppListModel fake.
   */
  class FakeAndroidAppListModel extends EventTarget {
    /**
     * @param {!Array<string>} appNames List app names to be added.
     */
    constructor(appNames) {
      super();
      this.apps_ = [];
      for (let i = 0; i < appNames.length; i++) {
        this.apps_.push({
          name: appNames[i],
          packageName: '',
          activityName: '',
        });
      }
    }

    /**
     * @return {number} Number of picker apps.
     */
    length() {
      return this.apps_.length;
    }

    /**
     * @param {number} index Index of the picker app to be retrieved.
     * @return {chrome.fileManagerPrivate.AndroidApp} The value of the
     *     |index|-th picker app.
     */
    item(index) {
      return this.apps_[index];
    }
  }

  const model = /** @type {!Object} */ (new FakeAndroidAppListModel(appNames));
  return /** @type {!AndroidAppListModel} */ (model);
}
