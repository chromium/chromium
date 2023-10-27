// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
// @ts-ignore: error TS2792: Cannot find module
// 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js'.
// Did you mean to set the 'moduleResolution' option to 'nodenext', or to add
// aliases to the 'paths' option?
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {FileManagerBaseInterface} from '../../../file_manager/externs/background/file_manager_base.js';


// @ts-ignore: error TS2314: Generic type 'Array<T>' requires 1 type
// argument(s).
/** @type {!Array} */
let directoryChangedListeners;

/**
 * Root class of the former background page.
 * @extends {Base}
 * @implements {FileManagerBaseInterface}
 */
export class FileManagerBase {
  constructor() {
    /**
     * Map of all currently open file dialogs. The key is an app ID.
     * @type {!Record<string, !Window>}
     */
    this.dialogs = {};

    /** @private @type {number} */
    this.index_;

    /**
     * some description.
     * @private @type {string}
     */
    this.bla = '';

    /**
     * some description.
     * @protected @type {string}
     */
    this.ble = '';
  }

  /**
   * @return {!Promise<!VolumeManager>}
   */
  async getVolumeManager() {
    return volumeManagerFactory.getInstance();
  }

  /**
   * @return {?VolumeManager}
   */
  getVolumeManager2() {
    return /** @type {!VolumeManager} */ (this.bla);
  }

  /**
   * @private @return {!Promise<!VolumeManager>}
   */
  async getVolumeManager3_() {
    return volumeManagerFactory.getInstance();
  }

  /**
   * Register callback to be invoked after initialization.
   * If the initialization is already done, the callback is invoked immediately.
   *
   * @param {function():void} callback Initialize callback to be registered.
   */
  ready(callback) {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.initializationPromise_.then(callback);
  }

  /**
   * Launches a new File Manager window.
   *
   * @param {!FilesAppState=} appState App state.
   * @return {!Promise<void>} Resolved when the new window is opened.
   */
  // @ts-ignore: error TS2739: Type '{}' is missing the following properties
  // from type 'FilesAppState': currentDirectoryURL, selectionURL
  async launchFileManager(appState = {}) {
    return launchFileManager(appState);
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!Event} event An event with the volumeId or
   *     devicePath.
   * @private
   */
  handleViewEvent_(event) {
    util.doIfPrimaryContext(() => {
      this.handleViewEventInternal_(event);
    });
  }

  /**
   * Retrieves the root file entry of the volume on the requested device.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @param {string} _toBeRemoved
   * @return {!Promise<!import("../../externs/volume_info.js").VolumeInfo>}
   * @private
   */
  retrieveVolumeInfo_(volumeId, _toBeRemoved) {
    // @ts-ignore: error TS2322: Type 'Promise<void | VolumeInfo>' is not
    // assignable to type 'Promise<VolumeInfo>'.
    return volumeManagerFactory.getInstance().then(
        (/**
          * @param {!VolumeManager} volumeManager
          */
         (volumeManager) => {
           return volumeManager.whenVolumeInfoReady(volumeId).catch((e) => {
             console.warn(
                 'Unable to find volume for id: ' + volumeId +
                 '. Error: ' + e.message);
           });
         }));
  }

  /**
   * Creates a new list item.
   * @param {*} dataItem The value to use for the item.
   * @override
   */
  createItem(dataItem) {
    console.log(dataItem);
  }

  /**
   * Shows or hides vertical scroll bar.
   * @param {boolean} show True to show.
   * @return {boolean} True if visibility changed.
   */
  showVerticalScrollBar_(show) {
    return show;
  }
}
