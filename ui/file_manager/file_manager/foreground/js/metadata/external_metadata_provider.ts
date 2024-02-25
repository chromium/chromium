// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {getEntryProperties} from '../../../common/js/api.js';
import {unwrapEntry} from '../../../common/js/entry_utils.js';

import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import type {MetadataRequest} from './metadata_request.js';

/**
 * Metadata provider for FileEntry#getMetadata.
 * TODO(hirono): Rename thumbnailUrl with externalThumbnailUrl.
 * @final
 */
export class ExternalMetadataProvider extends MetadataProvider {
  static readonly PROPERTY_NAMES = [
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

  constructor() {
    super(ExternalMetadataProvider.PROPERTY_NAMES);
  }

  override async get(requests: MetadataRequest[]): Promise<MetadataItem[]> {
    if (!requests.length) {
      return [];
    }
    const entries = requests.map(request => {
      return unwrapEntry(request.entry);
    }) as Entry[];
    const nameSet: Set<string> = new Set();
    for (const request of requests) {
      for (const name of request.names) {
        nameSet.add(name);
      }
    }
    const properties = Array.from(nameSet);

    try {
      const props = properties as chrome.fileManagerPrivate.EntryPropertyName[];
      const results = await getEntryProperties(entries, props);
      assert(results);
      return this.convertResults_(requests, nameSet, results);
    } catch (error: any) {
      return requests.map(() => new MetadataItem());
    }
  }

  /**
   * @param nameSet A set of property names that will be used to copy the value
   *     from |propertiesList|.
   */
  private convertResults_(
      requests: MetadataRequest[], nameSet: Set<string>,
      propertiesList: chrome.fileManagerPrivate.EntryProperties[]):
      MetadataItem[] {
    const results = [];
    for (let i = 0; i < propertiesList.length; i++) {
      const prop = propertiesList[i]!;
      const item = new MetadataItem();
      item.alternateUrl = prop.alternateUrl;
      item.availableOffline = prop.availableOffline;
      item.availableWhenMetered = prop.availableWhenMetered;
      if (prop.contentMimeType !== undefined ||
          nameSet.has('contentMimeType')) {
        item.contentMimeType = prop.contentMimeType || '';
      }
      item.croppedThumbnailUrl = prop.croppedThumbnailUrl;
      if (prop.customIconUrl !== undefined || nameSet.has('customIconUrl')) {
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
      if (prop.size !== undefined || nameSet.has('size')) {
        item.size = requests[i]!.entry.isFile ? (prop.size || 0) : -1;
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
