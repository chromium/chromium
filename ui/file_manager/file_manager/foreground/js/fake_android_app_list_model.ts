// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {AndroidAppListModel} from './android_app_list_model.js';

/**
 * @param appNames List app names to be added.
 * @return fake for unittests.
 */
export function createFakeAndroidAppListModel(appNames: string[]):
    AndroidAppListModel {
  /**
   * AndroidAppListModel fake.
   */
  class FakeAndroidAppListModel extends EventTarget {
    private apps_: chrome.fileManagerPrivate.AndroidApp[] = [];
    /**
     * @param appNames List app names to be added.
     */
    constructor(appNames: string[]) {
      super();
      for (const appName of appNames) {
        this.apps_.push({
          name: appName,
          packageName: '',
          activityName: '',
        } as chrome.fileManagerPrivate.AndroidApp);
      }
    }

    /**
     * @return Number of picker apps.
     */
    length(): number {
      return this.apps_.length;
    }

    /**
     * @param index Index of the picker app to be retrieved.
     * @return The value of the
     *     |index|-th picker app.
     */
    item(index: number): chrome.fileManagerPrivate.AndroidApp|undefined {
      return this.apps_[index];
    }
  }

  const model = new FakeAndroidAppListModel(appNames);
  return model as unknown as AndroidAppListModel;
}
