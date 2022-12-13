// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {util} from '../../../common/js/util.js';
import {VolumeManagerCommon} from '../../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../../externs/volume_manager.js';

import {ContentMetadataProvider} from './content_metadata_provider.js';
import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';

/** @final */
export class MultiMetadataProvider extends MetadataProvider {
  /**
   * @param {!FileSystemMetadataProvider} fileSystemMetadataProvider
   * @param {!ExternalMetadataProvider} externalMetadataProvider
   * @param {!ContentMetadataProvider} contentMetadataProvider
   * @param {!DlpMetadataProvider} dlpMetadataProvider
   * @param {!VolumeManager} volumeManager
   */
  constructor(
      fileSystemMetadataProvider, externalMetadataProvider,
      contentMetadataProvider, dlpMetadataProvider, volumeManager) {
    super(FileSystemMetadataProvider.PROPERTY_NAMES
              .concat(ExternalMetadataProvider.PROPERTY_NAMES)
              .concat(ContentMetadataProvider.PROPERTY_NAMES)
              .concat(DlpMetadataProvider.PROPERTY_NAMES));

    /** @private @const {!FileSystemMetadataProvider} */
    this.fileSystemMetadataProvider_ = fileSystemMetadataProvider;

    /** @private @const {!ExternalMetadataProvider} */
    this.externalMetadataProvider_ = externalMetadataProvider;

    /** @private @const {!ContentMetadataProvider} */
    this.contentMetadataProvider_ = contentMetadataProvider;

    /** @private @const {!DlpMetadataProvider} */
    this.dlpMetadataProvider_ = dlpMetadataProvider;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;
  }

  /**
   * Obtains metadata for entries.
   * @param {!Array<!MetadataRequest>} requests
   * @return {!Promise<!Array<!MetadataItem>>}
   */
  get(requests) {
    const fileSystemRequests = [];
    const externalRequests = [];
    const contentRequests = [];
    const fallbackContentRequests = [];
    const dlpRequests = [];
    requests.forEach(request => {
      // Group property names.
      const fileSystemPropertyNames = [];
      const externalPropertyNames = [];
      const contentPropertyNames = [];
      const fallbackContentPropertyNames = [];
      const dlpPropertyNames = [];
      for (let i = 0; i < request.names.length; i++) {
        const name = request.names[i];
        const isFileSystemProperty =
            FileSystemMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isExternalProperty =
            ExternalMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isContentProperty =
            ContentMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isDlpProperty =
            DlpMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
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
      const addRequests = (list, names) => {
        if (names.length) {
          list.push(new MetadataRequest(request.entry, names));
        }
      };
      if (volumeInfo && !util.isTrashEntry(request.entry) &&
          (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE ||
           volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED)) {
        // Because properties can be out of sync just after sync completion
        // even if 'dirty' is false, it refers 'present' here to switch the
        // content and the external providers.
        if (fallbackContentPropertyNames.length &&
            externalPropertyNames.indexOf('present') === -1) {
          externalPropertyNames.push('present');
        }
        addRequests(externalRequests, externalPropertyNames);
        addRequests(contentRequests, contentPropertyNames);
        addRequests(fallbackContentRequests, fallbackContentPropertyNames);
      } else if (
          volumeInfo &&
          volumeInfo.volumeType ===
              VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER) {
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
    });

    const get = (provider, inRequests) => {
      return provider.get(inRequests).then(results => {
        return {
          requests: inRequests,
          results: results,
        };
      });
    };
    const fileSystemPromise =
        get(this.fileSystemMetadataProvider_, fileSystemRequests);
    const externalPromise =
        get(this.externalMetadataProvider_, externalRequests);
    const contentPromise = get(this.contentMetadataProvider_, contentRequests);
    const fallbackContentPromise = externalPromise.then(requestsAndResults => {
      const requests = requestsAndResults.requests;
      const results = requestsAndResults.results;
      const dirtyMap = [];
      for (let i = 0; i < results.length; i++) {
        dirtyMap[requests[i].entry.toURL()] = results[i].present;
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
          const integratedResults = {};
          for (let i = 0; i < resultsList.length; i++) {
            const inRequests = resultsList[i].requests;
            const results = resultsList[i].results;
            assert(inRequests.length === results.length);
            for (let j = 0; j < results.length; j++) {
              const url = inRequests[j].entry.toURL();
              integratedResults[url] =
                  integratedResults[url] || new MetadataItem();
              for (const name in results[j]) {
                integratedResults[url][name] = results[j][name];
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

/**
 * Property names of documents-provider files which we should get from
 * ExternalMetadataProvider.
 *
 * We should NOT use ExternalMetadataProvider.PROPERTY_NAMES for
 * documents-provider files, since ExternalMetadataProvider zero-fills all
 * requested properties (e.g. 'size' is initialized to '0 bytes' even when size
 * is not acquired by chrome.fileManagerPrivate.getEntryProperties) and the
 * zero-filled property can overwrite a valid property which is already
 * acquired from FileSystemMetadataProvider.
 *
 * @const {!Array<string>}
 */
MultiMetadataProvider.DOCUMENTS_PROVIDER_EXTERNAL_PROPERTY_NAMES = [
  'canCopy',
  'canDelete',
  'canRename',
  'canAddChildren',
  'modificationTime',
  'size',
];
