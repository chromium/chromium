// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Metadata provider for FileEntry#getMetadata.
 * TODO(hirono): Rename thumbnailUrl with externalThumbnailUrl.
 * @final
 */
class ExternalMetadataProvider extends MetadataProvider {
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
      if (prop.alternateUrl !== undefined || nameMap['alternateUrl']) {
        item.alternateUrl = prop.alternateUrl;
      }
      if (prop.availableOffline !== undefined || nameMap['availableOffline']) {
        item.availableOffline = prop.availableOffline;
      }
      if (prop.availableWhenMetered !== undefined ||
          nameMap['availableWhenMetered']) {
        item.availableWhenMetered = prop.availableWhenMetered;
      }
      if (prop.contentMimeType !== undefined || nameMap['contentMimeType']) {
        item.contentMimeType = prop.contentMimeType || '';
      }
      if (prop.croppedThumbnailUrl !== undefined ||
          nameMap['croppedThumbnailUrl']) {
        item.croppedThumbnailUrl = prop.croppedThumbnailUrl;
      }
      if (prop.customIconUrl !== undefined || nameMap['customIconUrl']) {
        item.customIconUrl = prop.customIconUrl || '';
      }
      if (prop.dirty !== undefined || nameMap['dirty']) {
        item.dirty = prop.dirty;
      }
      if (prop.externalFileUrl !== undefined || nameMap['externalFileUrl']) {
        item.externalFileUrl = prop.externalFileUrl;
      }
      if (prop.hosted !== undefined || nameMap['hosted']) {
        item.hosted = prop.hosted;
      }
      if (prop.imageHeight !== undefined || nameMap['imageHeight']) {
        item.imageHeight = prop.imageHeight;
      }
      if (prop.imageRotation !== undefined || nameMap['imageRotation']) {
        item.imageRotation = prop.imageRotation;
      }
      if (prop.imageWidth !== undefined || nameMap['imageWidth']) {
        item.imageWidth = prop.imageWidth;
      }
      if (prop.modificationTime !== undefined || nameMap['modificationTime']) {
        item.modificationTime = new Date(prop.modificationTime);
      }
      if (prop.modificationByMeTime !== undefined ||
          nameMap['modificationByMeTime']) {
        item.modificationByMeTime = new Date(prop.modificationByMeTime);
      }
      if (prop.pinned !== undefined || nameMap['pinned']) {
        item.pinned = prop.pinned;
      }
      if (prop.present !== undefined || nameMap['present']) {
        item.present = prop.present;
      }
      if (prop.shared !== undefined || nameMap['shared']) {
        item.shared = prop.shared;
      }
      if (prop.sharedWithMe !== undefined || nameMap['sharedWithMe']) {
        item.sharedWithMe = prop.sharedWithMe;
      }
      if (prop.size !== undefined || nameMap['size']) {
        item.size = requests[i].entry.isFile ? (prop.size || 0) : -1;
      }
      if (prop.thumbnailUrl !== undefined || nameMap['thumbnailUrl']) {
        item.thumbnailUrl = prop.thumbnailUrl;
      }
      if (prop.canCopy !== undefined || nameMap['canCopy']) {
        item.canCopy = prop.canCopy;
      }
      if (prop.canDelete !== undefined || nameMap['canDelete']) {
        item.canDelete = prop.canDelete;
      }
      if (prop.canRename !== undefined || nameMap['canRename']) {
        item.canRename = prop.canRename;
      }
      if (prop.canAddChildren !== undefined || nameMap['canAddChildren']) {
        item.canAddChildren = prop.canAddChildren;
      }
      if (prop.canShare !== undefined || nameMap['canShare']) {
        item.canShare = prop.canShare;
      }
      if (prop.isMachineRoot !== undefined || nameMap['isMachineRoot']) {
        item.isMachineRoot = prop.isMachineRoot;
      }
      if (prop.isExternalMedia !== undefined || nameMap['isExternalMedia']) {
        item.isExternalMedia = prop.isExternalMedia;
      }
      if (prop.isArbitrarySyncFolder !== undefined ||
          nameMap['isArbitrarySyncFolder']) {
        item.isArbitrarySyncFolder = prop.isArbitrarySyncFolder;
      }
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
  'isMachineRoot',
  'isExternalMedia',
  'isArbitrarySyncFolder',
];
