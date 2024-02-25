// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {isTrashEntry} from '../../../common/js/entry_utils.js';
import {VolumeType} from '../../../common/js/volume_manager_types.js';

import {ContentMetadataProvider} from './content_metadata_provider.js';
import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataItem, type MetadataKey} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

/** @final */
export class MultiMetadataProvider extends MetadataProvider {
  /**
   * Property names of documents-provider files which we should get from
   * ExternalMetadataProvider.
   *
   * We should NOT use ExternalMetadataProvider.PROPERTY_NAMES for
   * documents-provider files, since ExternalMetadataProvider zero-fills all
   * requested properties (e.g. 'size' is initialized to '0 bytes' even when
   * size is not acquired by chrome.fileManagerPrivate.getEntryProperties) and
   * the zero-filled property can overwrite a valid property which is already
   * acquired from FileSystemMetadataProvider.
   */
  static readonly DOCUMENTS_PROVIDER_EXTERNAL_PROPERTY_NAMES: MetadataKey[] = [
    'canCopy',
    'canDelete',
    'canRename',
    'canAddChildren',
    'modificationTime',
    'size',
  ];

  constructor(
      private readonly fileSystemMetadataProvider_: FileSystemMetadataProvider,
      private readonly externalMetadataProvider_: ExternalMetadataProvider,
      private readonly contentMetadataProvider_: ContentMetadataProvider,
      private readonly dlpMetadataProvider_: DlpMetadataProvider,
      private readonly volumeManager_: VolumeManager) {
    super(FileSystemMetadataProvider.PROPERTY_NAMES.concat(
        ExternalMetadataProvider.PROPERTY_NAMES,
        ContentMetadataProvider.PROPERTY_NAMES,
        DlpMetadataProvider.PROPERTY_NAMES));
  }

  /**
   * Obtains metadata for entries.
   */
  get(requests: MetadataRequest[]): Promise<MetadataItem[]> {
    const fileSystemRequests: MetadataRequest[] = [];
    const externalRequests: MetadataRequest[] = [];
    const contentRequests: MetadataRequest[] = [];
    const fallbackContentRequests: MetadataRequest[] = [];
    const dlpRequests: MetadataRequest[] = [];
    for (const request of requests) {
      // Group property names.
      const fileSystemPropertyNames: MetadataKey[] = [];
      const externalPropertyNames: MetadataKey[] = [];
      const contentPropertyNames: MetadataKey[] = [];
      const fallbackContentPropertyNames: MetadataKey[] = [];
      const dlpPropertyNames: MetadataKey[] = [];
      for (const name of request.names) {
        const isFileSystemProperty =
            FileSystemMetadataProvider.PROPERTY_NAMES.includes(name);
        const isExternalProperty =
            ExternalMetadataProvider.PROPERTY_NAMES.includes(name);
        const isContentProperty =
            ContentMetadataProvider.PROPERTY_NAMES.includes(name);
        const isDlpProperty = DlpMetadataProvider.PROPERTY_NAMES.includes(name);
        assert(
            isFileSystemProperty || isExternalProperty || isContentProperty ||
            isDlpProperty);
        assert(!(isFileSystemProperty && isContentProperty));
        // If the property can be obtained both from ExternalProvider and from
        // ContentProvider, we can obtain the property from ExternalProvider
        // without fetching file content. On the other hand, the values from
        // ExternalProvider may be out of sync if the file is 'dirty'. Thus we
        // fallback to ContentProvider if the file is dirty. See below.
        if (isExternalProperty && isContentProperty) {
          externalPropertyNames.push(name);
          fallbackContentPropertyNames.push(name);
          continue;
        }
        if (isFileSystemProperty) {
          fileSystemPropertyNames.push(name);
        }
        if (isExternalProperty) {
          externalPropertyNames.push(name);
        }
        if (isContentProperty) {
          contentPropertyNames.push(name);
        }
        if (isDlpProperty) {
          dlpPropertyNames.push(name);
        }
      }
      const volumeInfo = this.volumeManager_.getVolumeInfo(request.entry);
      const addRequests = (list: MetadataRequest[], names: MetadataKey[]) => {
        if (names.length) {
          list.push(new MetadataRequest(request.entry, names));
        }
      };
      if (volumeInfo && !isTrashEntry(request.entry) &&
          (volumeInfo.volumeType === VolumeType.DRIVE ||
           volumeInfo.volumeType === VolumeType.PROVIDED)) {
        // Because properties can be out of sync just after sync completion
        // even if 'dirty' is false, it refers 'present' here to switch the
        // content and the external providers.
        if (fallbackContentPropertyNames.length &&
            !externalPropertyNames.includes('present')) {
          externalPropertyNames.push('present');
        }
        addRequests(externalRequests, externalPropertyNames);
        addRequests(contentRequests, contentPropertyNames);
        addRequests(fallbackContentRequests, fallbackContentPropertyNames);
      } else if (
          volumeInfo &&
          volumeInfo.volumeType === VolumeType.DOCUMENTS_PROVIDER) {
        // When using a documents provider, we need to discard:
        // - contentRequests: since the content sniffing code
        //   can't resolve the file path in the MediaGallery API. See
        //   crbug.com/942417
        // - fileSystemRequests: because it does not correctly handle unknown
        //   file size, which DocumentsProvider files may report (all filesystem
        //   request fields are retrieved using external requests instead).
        addRequests(
            externalRequests,
            MultiMetadataProvider.DOCUMENTS_PROVIDER_EXTERNAL_PROPERTY_NAMES);
      } else {
        addRequests(fileSystemRequests, fileSystemPropertyNames);
        addRequests(
            contentRequests,
            contentPropertyNames.concat(fallbackContentPropertyNames));
      }
      addRequests(dlpRequests, dlpPropertyNames);
    }

    const get =
        async (provider: MetadataProvider, inRequests: MetadataRequest[]) => {
      const results = await provider.get(inRequests);
      return {inRequests, results};
    };
    const fileSystemPromise =
        get(this.fileSystemMetadataProvider_, fileSystemRequests);
    const externalPromise =
        get(this.externalMetadataProvider_, externalRequests);
    const contentPromise = get(this.contentMetadataProvider_, contentRequests);
    const fallbackContentPromise = externalPromise.then(requestsAndResults => {
      const requests = requestsAndResults.inRequests;
      const results = requestsAndResults.results;
      const dirtyMap: {[key: string]: boolean|undefined} = {};
      for (const [i, result] of results.entries()) {
        dirtyMap[requests[i]!.entry.toURL()] = result.present;
      }
      return get(
          this.contentMetadataProvider_,
          fallbackContentRequests.filter(request => {
            return dirtyMap[request.entry.toURL()];
          }));
    });
    const dlpPromise = get(this.dlpMetadataProvider_, dlpRequests);

    // Merge results.
    return Promise
        .all([
          fileSystemPromise,
          externalPromise,
          contentPromise,
          fallbackContentPromise,
          dlpPromise,
        ])
        .then(resultsList => {
          const integratedResults: {[key: string]: MetadataItem} = {};
          for (const result of resultsList) {
            const {inRequests, results} = result;
            assert(inRequests.length === results.length);
            for (const [i, result] of results.entries()) {
              const url = inRequests[i]!.entry.toURL();
              integratedResults[url] =
                  integratedResults[url] || new MetadataItem();
              for (const name in result) {
                // `undefined` is the intersection of all possible properties of
                // MetadataItem.
                integratedResults[url]![name as MetadataKey] =
                    result![name as MetadataKey] as undefined;
              }
            }
          }
          return requests.map(request => {
            return integratedResults[request.entry.toURL()] ||
                new MetadataItem();
          });
        });
  }
}
