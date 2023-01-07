// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

/**
 * File path component.
 *
 * File path can be represented as a series of path components. Each component
 * has its name used as a visible label and URL which point to the component
 * in the path.
 * PathComponent.computeComponentsFromEntry computes an array of PathComponent
 * of the given entry.
 */
export class PathComponent {
  /**
   * @param {string} name Name.
   * @param {string} url Url.
   * @param {FilesAppEntry=} opt_fakeEntry Fake entry should be set when
   *     this component represents fake entry.
   */
  constructor(name, url, opt_fakeEntry) {
    this.name = name;
    this.url_ = url;
    this.fakeEntry_ = opt_fakeEntry || null;
  }

  /**
   * Resolve an entry of the component.
   * @return {!Promise<!Entry|!FilesAppEntry>} A promise which is
   *     resolved with an entry.
   */
  resolveEntry() {
    if (this.fakeEntry_) {
      return /** @type {!Promise<!Entry|!FilesAppEntry>} */ (
          Promise.resolve(this.fakeEntry_));
    } else {
      return new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, this.url_));
    }
  }

  /**
   * Computes path components for the path of entry.
   * @param {!Entry|!FilesAppEntry} entry An entry.
   * @return {!Array<!PathComponent>} Components.
   */
  static computeComponentsFromEntry(entry, volumeManager) {
    /**
     * Replace the root directory name at the end of a url.
     * The input, |url| is a displayRoot URL of a Drive volume like
     * filesystem:chrome-extension://....foo.com-hash/root
     * The output is like:
     * filesystem:chrome-extension://....foo.com-hash/other
     *
     * @param {string} url which points to a volume display root
     * @param {string} newRoot new root directory name
     * @return {string} new URL with the new root directory name
     */
    const replaceRootName = (url, newRoot) => {
      return url.slice(0, url.length - '/root'.length) + newRoot;
    };

    const components = [];
    const locationInfo = volumeManager.getLocationInfo(entry);

    if (!locationInfo) {
      return components;
    }

    if (util.isFakeEntry(entry)) {
      components.push(new PathComponent(
          util.getEntryLabel(locationInfo, entry), entry.toURL(),
          /** @type {!FakeEntry} */ (entry)));
      return components;
    }

    // Add volume component.
    let displayRootUrl = locationInfo.volumeInfo.displayRoot.toURL();
    let displayRootFullPath = locationInfo.volumeInfo.displayRoot.fullPath;

    const prefixEntry = locationInfo.volumeInfo.prefixEntry;
    if (prefixEntry) {
      components.push(new PathComponent(
          prefixEntry.name, prefixEntry.toURL(), prefixEntry));
    }
    if (locationInfo.rootType ===
        VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME) {
      // DriveFS shared items are in either of:
      // <drivefs>/.files-by-id/<id>/<item>
      // <drivefs>/.shortcut-targets-by-id/<id>/<item>
      const match =
          entry.fullPath.match(/^\/\.(files|shortcut-targets)-by-id\/.+?\//);
      if (match) {
        displayRootFullPath = match[0];
      } else {
        console.warn('Unexpected shared DriveFS path: ', entry.fullPath);
      }
      displayRootUrl = replaceRootName(displayRootUrl, displayRootFullPath);
      const sharedWithMeFakeEntry =
          locationInfo.volumeInfo
              .fakeEntries[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
      components.push(new PathComponent(
          str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
          sharedWithMeFakeEntry.toURL(), sharedWithMeFakeEntry));
    } else if (
        locationInfo.rootType === VolumeManagerCommon.RootType.SHARED_DRIVE) {
      displayRootUrl = replaceRootName(
          displayRootUrl, VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH);
      components.push(new PathComponent(
          util.getRootTypeLabel(locationInfo), displayRootUrl));
    } else if (
        locationInfo.rootType === VolumeManagerCommon.RootType.COMPUTER) {
      displayRootUrl = replaceRootName(
          displayRootUrl, VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH);
      components.push(new PathComponent(
          util.getRootTypeLabel(locationInfo), displayRootUrl));
    } else {
      components.push(new PathComponent(
          util.getRootTypeLabel(locationInfo), displayRootUrl));
    }

    // Get relative path to display root (e.g. /root/foo/bar -> foo/bar).
    let relativePath = entry.fullPath.slice(displayRootFullPath.length);
    if (entry.fullPath.startsWith(
            VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH)) {
      relativePath = entry.fullPath.slice(
          VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH.length);
    } else if (entry.fullPath.startsWith(
                   VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH)) {
      relativePath = entry.fullPath.slice(
          VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH.length);
    }
    if (relativePath.indexOf('/') === 0) {
      relativePath = relativePath.slice(1);
    }
    if (relativePath.length === 0) {
      return components;
    }

    // currentUrl should be without trailing slash.
    let currentUrl = /^.+\/$/.test(displayRootUrl) ?
        displayRootUrl.slice(0, displayRootUrl.length - 1) :
        displayRootUrl;

    // Add directory components to the target path.
    const paths = relativePath.split('/');
    for (let i = 0; i < paths.length; i++) {
      currentUrl += '/' + encodeURIComponent(paths[i]);
      let path = paths[i];
      if (i === 0 &&
          locationInfo.rootType === VolumeManagerCommon.RootType.DOWNLOADS) {
        if (path === 'Downloads') {
          path = str('DOWNLOADS_DIRECTORY_LABEL');
        }
        if (path === 'PvmDefault') {
          path = str('PLUGIN_VM_DIRECTORY_LABEL');
        }
        if (path === 'Camera') {
          path = str('CAMERA_DIRECTORY_LABEL');
        }
      }
      components.push(new PathComponent(path, currentUrl));
    }

    return components;
  }
}
