// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isFakeEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {getEntryLabel, getRootTypeLabel, str} from '../../common/js/translations.js';
import {COMPUTERS_DIRECTORY_PATH, RootType, SHARED_DRIVES_DIRECTORY_PATH} from '../../common/js/volume_manager_types.js';

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
   * @param name Name.
   * @param url Url.
   * @param fakeEntry Fake entry should be set when this component represents
   *     fake entry.
   */
  constructor(
      public readonly name: string, private readonly url_: string,
      private fakeEntry_?: FilesAppEntry) {}

  /**
   * Resolve an entry of the component.
   * @return A promise which is resolved with an entry.
   */
  resolveEntry(): Promise<Entry|FilesAppEntry> {
    if (this.fakeEntry_) {
      return Promise.resolve(this.fakeEntry_);
    } else {
      return new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, this.url_));
    }
  }

  /**
   * Returns the key of this component (its URL).
   */
  getKey() {
    return this.url_;
  }

  /**
   * Computes path components for the path of entry.
   * @param entry An entry.
   * @return Components.
   */
  static computeComponentsFromEntry(
      entry: Entry|FilesAppEntry,
      volumeManager: VolumeManager): PathComponent[] {
    /**
     * Replace the root directory name at the end of a url.
     * The input, |url| is a displayRoot URL of a Drive volume like
     * filesystem:chrome-extension://....foo.com-hash/root
     * The output is like:
     * filesystem:chrome-extension://....foo.com-hash/other
     *
     * @param url which points to a volume display root
     * @param newRoot new root directory name
     * @return new URL with the new root directory name
     */
    const replaceRootName = (url: string, newRoot: string): string => {
      return url.slice(0, url.length - '/root'.length) + newRoot;
    };

    const components: PathComponent[] = [];
    const locationInfo = volumeManager.getLocationInfo(entry);

    if (!locationInfo) {
      return components;
    }

    if (isFakeEntry(entry)) {
      components.push(new PathComponent(
          getEntryLabel(locationInfo, entry), entry.toURL(), entry));
      return components;
    }

    // Add volume component.
    const volumeInfo = locationInfo.volumeInfo;
    if (!volumeInfo) {
      return components;
    }

    let displayRootUrl = volumeInfo.displayRoot.toURL();
    let displayRootFullPath = volumeInfo.displayRoot.fullPath;

    const prefixEntry = volumeInfo.prefixEntry;
    // Directories under Drive Fake Root can return the fake root entry list as
    // prefix entry, but we will never show "Google Drive" as the prefix in the
    // breadcrumb.
    if (prefixEntry && prefixEntry.rootType !== RootType.DRIVE_FAKE_ROOT) {
      components.push(new PathComponent(
          prefixEntry.name, prefixEntry.toURL(), prefixEntry));
    }
    if (locationInfo.rootType === RootType.DRIVE_SHARED_WITH_ME) {
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
          volumeInfo.fakeEntries[RootType.DRIVE_SHARED_WITH_ME];
      if (sharedWithMeFakeEntry) {
        components.push(new PathComponent(
            str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
            sharedWithMeFakeEntry.toURL(), sharedWithMeFakeEntry));
      }
    } else if (locationInfo.rootType === RootType.SHARED_DRIVE) {
      displayRootUrl =
          replaceRootName(displayRootUrl, SHARED_DRIVES_DIRECTORY_PATH);
      components.push(
          new PathComponent(getRootTypeLabel(locationInfo), displayRootUrl));
    } else if (locationInfo.rootType === RootType.COMPUTER) {
      displayRootUrl =
          replaceRootName(displayRootUrl, COMPUTERS_DIRECTORY_PATH);
      components.push(
          new PathComponent(getRootTypeLabel(locationInfo), displayRootUrl));
    } else {
      components.push(
          new PathComponent(getRootTypeLabel(locationInfo), displayRootUrl));
    }

    // Get relative path to display root (e.g. /root/foo/bar -> foo/bar).
    let relativePath = entry.fullPath.slice(displayRootFullPath.length);
    if (entry.fullPath.startsWith(SHARED_DRIVES_DIRECTORY_PATH)) {
      relativePath = entry.fullPath.slice(SHARED_DRIVES_DIRECTORY_PATH.length);
    } else if (entry.fullPath.startsWith(COMPUTERS_DIRECTORY_PATH)) {
      relativePath = entry.fullPath.slice(COMPUTERS_DIRECTORY_PATH.length);
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
      currentUrl += '/' + encodeURIComponent(paths[i]!);
      let path = paths[i]!;
      if (i === 0 && locationInfo.rootType === RootType.DOWNLOADS) {
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
