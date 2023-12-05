// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {TypeList} from '../../common/js/files_app_state.js';
import {addAndroidApps} from '../../state/ducks/android_apps.js';
import {getStore} from '../../state/store.js';

/**
 * Model for managing a list of Android apps.
 */
export class AndroidAppListModel extends EventTarget {
  private apps_: chrome.fileManagerPrivate.AndroidApp[] = [];
  /**
   * @param showAndroidPickerApps Whether to show picker apps in file
   *     selector.
   * @param includeAllFiles Corresponds to LaunchParam.includeAllFiles
   * @param typeList Corresponds to LaunchParam.typeList
   */
  constructor(
      showAndroidPickerApps: boolean, includeAllFiles: boolean,
      typeList: TypeList[]) {
    super();

    if (!showAndroidPickerApps) {
      return;
    }

    let extensions: string[] = [];
    if (!includeAllFiles) {
      for (const type of typeList) {
        extensions = extensions.concat(type.extensions);
      }
    }

    chrome.fileManagerPrivate.getAndroidPickerApps(extensions, apps => {
      this.apps_ = apps;
      getStore().dispatch(addAndroidApps({apps}));
      this.dispatchEvent(new CustomEvent('permuted'));
    });
  }

  /**
   * @return Number of picker apps.
   */
  length(): number {
    return this.apps_.length;
  }

  /**
   * @param index Index of the picker app to be retrieved.
   * @return The value of the |index|-th
   *     picker app.
   */
  item(index: number): chrome.fileManagerPrivate.AndroidApp|undefined {
    return this.apps_[index];
  }
}
