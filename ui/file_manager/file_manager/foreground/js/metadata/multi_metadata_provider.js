// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {isTrashEntry} from '../../../common/js/entry_utils.js';
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

    /** @private @const @type {!FileSystemMetadataProvider} */
    this.fileSystemMetadataProvider_ = fileSystemMetadataProvider;

    /** @private @const @type {!ExternalMetadataProvider} */
    this.externalMetadataProvider_ = externalMetadataProvider;

    /** @private @const @type {!ContentMetadataProvider} */
    this.contentMetadataProvider_ = contentMetadataProvider;

    /** @private @const @type {!DlpMetadataProvider} */
    this.dlpMetadataProvider_ = dlpMetadataProvider;

    /** @private @const @type {!VolumeManager} */
    this.volumeManager_ = volumeManager;
  }

  /**
   * Obtains metadata for entries.
   * @param {!Array<!MetadataRequest>} requests
   * @return {!Promise<!Array<!MetadataItem>>}
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class
  // 'MetadataProvider'.
  get(requests) {
    // @ts-ignore: error TS7034: Variable 'fileSystemRequests' implicitly has
    // type 'any[]' in some locations where its type cannot be determined.
    const fileSystemRequests = [];
    // @ts-ignore: error TS7034: Variable 'externalRequests' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
    const externalRequests = [];
    // @ts-ignore: error TS7034: Variable 'contentRequests' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
    const contentRequests = [];
    // @ts-ignore: error TS7034: Variable 'fallbackContentRequests' implicitly
    // has type 'any[]' in some locations where its type cannot be determined.
    const fallbackContentRequests = [];
    // @ts-ignore: error TS7034: Variable 'dlpRequests' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
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
            // @ts-ignore: error TS2345: Argument of type 'string | undefined'
            // is not assignable to parameter of type 'string'.
            FileSystemMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isExternalProperty =
            // @ts-ignore: error TS2345: Argument of type 'string | undefined'
            // is not assignable to parameter of type 'string'.
            ExternalMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isContentProperty =
            // @ts-ignore: error TS2345: Argument of type 'string | undefined'
            // is not assignable to parameter of type 'string'.
            ContentMetadataProvider.PROPERTY_NAMES.indexOf(name) !== -1;
        const isDlpProperty =
            // @ts-ignore: error TS2345: Argument of type 'string | undefined'
            // is not assignable to parameter of type 'string'.
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
      // @ts-ignore: error TS7006: Parameter 'names' implicitly has an 'any'
      // type.
      const addRequests = (list, names) => {
        if (names.length) {
          list.push(new MetadataRequest(request.entry, names));
        }
      };
      if (volumeInfo && !isTrashEntry(request.entry) &&
          (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE ||
           volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED)) {
        // Because properties can be out of sync just after sync completion
        // even if 'dirty' is false, it refers 'present' here to switch the
        // content and the external providers.
        if (fallbackContentPropertyNames.length &&
            externalPropertyNames.indexOf('present') === -1) {
          externalPropertyNames.push('present');
        }
        // @ts-ignore: error TS7005: Variable 'externalRequests' implicitly has
        // an 'any[]' type.
        addRequests(externalRequests, externalPropertyNames);
        // @ts-ignore: error TS7005: Variable 'contentRequests' implicitly has
        // an 'any[]' type.
        addRequests(contentRequests, contentPropertyNames);
        // @ts-ignore: error TS7005: Variable 'fallbackContentRequests'
        // implicitly has an 'any[]' type.
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
            // @ts-ignore: error TS7005: Variable 'externalRequests' implicitly
            // has an 'any[]' type.
            externalRequests,
            MultiMetadataProvider.DOCUMENTS_PROVIDER_EXTERNAL_PROPERTY_NAMES);
      } else {
        // @ts-ignore: error TS7005: Variable 'fileSystemRequests' implicitly
        // has an 'any[]' type.
        addRequests(fileSystemRequests, fileSystemPropertyNames);
        addRequests(
            // @ts-ignore: error TS7005: Variable 'contentRequests' implicitly
            // has an 'any[]' type.
            contentRequests,
            contentPropertyNames.concat(fallbackContentPropertyNames));
      }
      // @ts-ignore: error TS7005: Variable 'dlpRequests' implicitly has an
      // 'any[]' type.
      addRequests(dlpRequests, dlpPropertyNames);
    });

    // @ts-ignore: error TS7006: Parameter 'inRequests' implicitly has an 'any'
    // type.
    const get = (provider, inRequests) => {
      // @ts-ignore: error TS7006: Parameter 'results' implicitly has an 'any'
      // type.
      return provider.get(inRequests).then(results => {
        return {
          requests: inRequests,
          results: results,
        };
      });
    };
    const fileSystemPromise =
        // @ts-ignore: error TS7005: Variable 'fileSystemRequests' implicitly
        // has an 'any[]' type.
        get(this.fileSystemMetadataProvider_, fileSystemRequests);
    const externalPromise =
        // @ts-ignore: error TS7005: Variable 'externalRequests' implicitly has
        // an 'any[]' type.
        get(this.externalMetadataProvider_, externalRequests);
    // @ts-ignore: error TS7005: Variable 'contentRequests' implicitly has an
    // 'any[]' type.
    const contentPromise = get(this.contentMetadataProvider_, contentRequests);
    // @ts-ignore: error TS7006: Parameter 'requestsAndResults' implicitly has
    // an 'any' type.
    const fallbackContentPromise = externalPromise.then(requestsAndResults => {
      const requests = requestsAndResults.requests;
      const results = requestsAndResults.results;
      // @ts-ignore: error TS7034: Variable 'dirtyMap' implicitly has type
      // 'any[]' in some locations where its type cannot be determined.
      const dirtyMap = [];
      for (let i = 0; i < results.length; i++) {
        dirtyMap[requests[i].entry.toURL()] = results[i].present;
      }
      return get(
          this.contentMetadataProvider_,
          // @ts-ignore: error TS7005: Variable 'fallbackContentRequests'
          // implicitly has an 'any[]' type.
          fallbackContentRequests.filter(request => {
            // @ts-ignore: error TS7005: Variable 'dirtyMap' implicitly has an
            // 'any[]' type.
            return dirtyMap[request.entry.toURL()];
          }));
    });
    // @ts-ignore: error TS7005: Variable 'dlpRequests' implicitly has an
    // 'any[]' type.
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
              // @ts-ignore: error TS7053: Element implicitly has an 'any' type
              // because expression of type 'any' can't be used to index type
              // '{}'.
              integratedResults[url] =
                  // @ts-ignore: error TS7053: Element implicitly has an 'any'
                  // type because expression of type 'any' can't be used to
                  // index type '{}'.
                  integratedResults[url] || new MetadataItem();
              for (const name in results[j]) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'any' can't be used to index
                // type '{}'.
                integratedResults[url][name] = results[j][name];
              }
            }
          }
          return requests.map(request => {
            // @ts-ignore: error TS7053: Element implicitly has an 'any' type
            // because expression of type 'string' can't be used to index type
            // '{}'.
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
 * @const @type {!Array<string>}
 */
MultiMetadataProvider.DOCUMENTS_PROVIDER_EXTERNAL_PROPERTY_NAMES = [
  'canCopy',
  'canDelete',
  'canRename',
  'canAddChildren',
  'modificationTime',
  'size',
];
