// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

/**
 * Metadata provider for FileEntry#getMetadata.
 * TODO(hirono): Rename thumbnailUrl with externalThumbnailUrl.
 * @final
 */
export class ExternalMetadataProvider extends MetadataProvider {
  constructor() {
    super(ExternalMetadataProvider.PROPERTY_NAMES);
  }

  /** @override */
  get(requests) {
    if (!requests.length) {
      return Promise.resolve([]);
    }
    return new Promise(fulfill => {
      const entries = requests.map(request => {
        return request.entry;
      });
      const nameMap = {};
      for (let i = 0; i < requests.length; i++) {
        for (let j = 0; j < requests[i].names.length; j++) {
          nameMap[requests[i].names[j]] = true;
        }
      }
      chrome.fileManagerPrivate.getEntryProperties(
          entries, Object.keys(nameMap), results => {
            if (!chrome.runtime.lastError) {
              fulfill(this.convertResults_(requests, nameMap, assert(results)));
            } else {
              fulfill(requests.map(() => {
                return new MetadataItem();
              }));
            }
          });
    });
  }

  /**
   * @param {!Array<!MetadataRequest>} requests
   * @param {!Object<boolean>} nameMap A map of property names that will be used
   *     to copy the value from |propertiesList|.
   * @param {!Array<!chrome.fileManagerPrivate.EntryProperties>} propertiesList
   * @return {!Array<!MetadataItem>}
   */
  convertResults_(requests, nameMap, propertiesList) {
    const results = [];
    for (let i = 0; i < propertiesList.length; i++) {
      const prop = propertiesList[i];
      const item = new MetadataItem();
      item.alternateUrl = prop.alternateUrl;
      item.availableOffline = prop.availableOffline;
      item.availableWhenMetered = prop.availableWhenMetered;
      if (prop.contentMimeType !== undefined || nameMap['contentMimeType']) {
        item.contentMimeType = prop.contentMimeType || '';
      }
      item.croppedThumbnailUrl = prop.croppedThumbnailUrl;
      if (prop.customIconUrl !== undefined || nameMap['customIconUrl']) {
        item.customIconUrl = prop.customIconUrl || '';
      }
      item.dirty = prop.dirty;
      item.externalFileUrl = prop.externalFileUrl;
      item.hosted = prop.hosted;
      item.imageHeight = prop.imageHeight;
      item.imageRotation = prop.imageRotation;
      item.imageWidth = prop.imageWidth;
      if (prop.modificationTime !== undefined) {
        item.modificationTime = new Date(prop.modificationTime);
      }
      if (prop.modificationByMeTime !== undefined) {
        item.modificationByMeTime = new Date(prop.modificationByMeTime);
      }
      item.pinned = prop.pinned;
      item.present = prop.present;
      item.shared = prop.shared;
      item.sharedWithMe = prop.sharedWithMe;
      if (prop.size !== undefined || nameMap['size']) {
        item.size = requests[i].entry.isFile ? (prop.size || 0) : -1;
      }
      item.thumbnailUrl = prop.thumbnailUrl;
      item.canCopy = prop.canCopy;
      item.canDelete = prop.canDelete;
      item.canRename = prop.canRename;
      item.canAddChildren = prop.canAddChildren;
      item.canShare = prop.canShare;
      item.canPin = prop.canPin;
      item.isMachineRoot = prop.isMachineRoot;
      item.isExternalMedia = prop.isExternalMedia;
      item.isArbitrarySyncFolder = prop.isArbitrarySyncFolder;
      item.syncStatus = prop.syncStatus;
      item.progress = prop.progress;
      item.shortcut = prop.shortcut;
      results.push(item);
    }
    return results;
  }
}

/** @const {!Array<string>} */
ExternalMetadataProvider.PROPERTY_NAMES = [
  'alternateUrl',
  'availableOffline',
  'availableWhenMetered',
  'contentMimeType',
  'croppedThumbnailUrl',
  'customIconUrl',
  'dirty',
  'externalFileUrl',
  'hosted',
  'imageHeight',
  'imageRotation',
  'imageWidth',
  'modificationTime',
  'modificationByMeTime',
  'pinned',
  'present',
  'shared',
  'sharedWithMe',
  'size',
  'thumbnailUrl',
  'canCopy',
  'canDelete',
  'canRename',
  'canAddChildren',
  'canShare',
  'canPin',
  'isMachineRoot',
  'isExternalMedia',
  'isArbitrarySyncFolder',
  'syncStatus',
  'progress',
  'shortcut',
];
