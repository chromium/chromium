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
  // @ts-ignore: error TS7006: Parameter 'requests' implicitly has an 'any'
  // type.
  get(requests) {
    if (!requests.length) {
      return Promise.resolve([]);
    }
    return new Promise(fulfill => {
      // @ts-ignore: error TS7006: Parameter 'request' implicitly has an 'any'
      // type.
      const entries = requests.map(request => {
        return request.entry;
      });
      /** @type {Record<string, boolean>} */
      const nameMap = {};
      for (let i = 0; i < requests.length; i++) {
        for (let j = 0; j < requests[i].names.length; j++) {
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'any' can't be used to index type '{}'.
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
   * @param {!Record<string, boolean>} nameMap A map of property names that will
   *     be used to copy the value from |propertiesList|.
   * @param {!Array<!chrome.fileManagerPrivate.EntryProperties>} propertiesList
   * @return {!Array<!MetadataItem>}
   */
  convertResults_(requests, nameMap, propertiesList) {
    const results = [];
    for (let i = 0; i < propertiesList.length; i++) {
      const prop = propertiesList[i];
      const item = new MetadataItem();
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.alternateUrl = prop.alternateUrl;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.availableOffline = prop.availableOffline;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.availableWhenMetered = prop.availableWhenMetered;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      if (prop.contentMimeType !== undefined || nameMap['contentMimeType']) {
        // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
        item.contentMimeType = prop.contentMimeType || '';
      }
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.croppedThumbnailUrl = prop.croppedThumbnailUrl;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      if (prop.customIconUrl !== undefined || nameMap['customIconUrl']) {
        // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
        item.customIconUrl = prop.customIconUrl || '';
      }
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.dirty = prop.dirty;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.externalFileUrl = prop.externalFileUrl;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.hosted = prop.hosted;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.imageHeight = prop.imageHeight;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.imageRotation = prop.imageRotation;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.imageWidth = prop.imageWidth;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      if (prop.modificationTime !== undefined) {
        // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
        item.modificationTime = new Date(prop.modificationTime);
      }
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      if (prop.modificationByMeTime !== undefined) {
        // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
        item.modificationByMeTime = new Date(prop.modificationByMeTime);
      }
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.pinned = prop.pinned;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.present = prop.present;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.shared = prop.shared;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.sharedWithMe = prop.sharedWithMe;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      if (prop.size !== undefined || nameMap['size']) {
        // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
        item.size = requests[i].entry.isFile ? (prop.size || 0) : -1;
      }
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.thumbnailUrl = prop.thumbnailUrl;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canCopy = prop.canCopy;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canDelete = prop.canDelete;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canRename = prop.canRename;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canAddChildren = prop.canAddChildren;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canShare = prop.canShare;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.canPin = prop.canPin;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.isMachineRoot = prop.isMachineRoot;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.isExternalMedia = prop.isExternalMedia;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.isArbitrarySyncFolder = prop.isArbitrarySyncFolder;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.syncStatus = prop.syncStatus;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.progress = prop.progress;
      // @ts-ignore: error TS18048: 'prop' is possibly 'undefined'.
      item.shortcut = prop.shortcut;
      results.push(item);
    }
    return results;
  }
}

/** @const @type {!Array<string>} */
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
  'syncCompletedTime',
];
