// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {reportPromise} from '../../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../../externs/volume_manager.js';

import {ContentMetadataProvider} from './content_metadata_provider.js';
import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataRequest} from './metadata_request.js';
import {MultiMetadataProvider} from './multi_metadata_provider.js';

const entryA = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://A';
  },
});

const entryB = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://B';
  },
});

const entryC = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://C';
  },
});

const entryD = /** @type {!Entry} */ ({
  toURL: function() {
    return 'filesystem://D';
  },
});

const volumeManager = /** @type {!VolumeManager} */ ({
  // @ts-ignore: error TS7030: Not all code paths return a value.
  getVolumeInfo: function(entry) {
    if (entry.toURL() === 'filesystem://A') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      };
    } else if (entry.toURL() === 'filesystem://B') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DRIVE,
      };
    } else if (entry.toURL() === 'filesystem://C') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DRIVE,
      };
    } else if (entry.toURL() === 'filesystem://D') {
      return {
        volumeType: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      };
    }
    assertNotReached();
  },
});

/** @param {()=>void} callback */
export function testMultiMetadataProviderBasic(callback) {
  const model = new MultiMetadataProvider(
      /** @type {!FileSystemMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(1, requests.length);
          assertEquals('filesystem://A', requests[0].entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], requests[0].names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 0, 1), size: 1024}]);
        },
      }),
      /** @type {!ExternalMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(1, requests.length);
          assertEquals('filesystem://B', requests[0].entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], requests[0].names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 1, 2), size: 2048}]);
        },
      }),
      /** @type {!ContentMetadataProvider} */ ({
        get: function(requests) {
          if (requests.length === 0) {
            return Promise.resolve([]);
          }
          assertEquals(2, requests.length);
          assertEquals('filesystem://A', requests[0].entry.toURL());
          assertEquals('filesystem://B', requests[1].entry.toURL());
          assertArrayEquals(['contentThumbnailUrl'], requests[0].names);
          assertArrayEquals(['contentThumbnailUrl'], requests[1].names);
          return Promise.resolve([
            {contentThumbnailUrl: 'THUMBNAIL_URL_A'},
            {contentThumbnailUrl: 'THUMBNAIL_URL_B'},
          ]);
        },
      }),
      /** @type {!DlpMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      volumeManager);

  reportPromise(
      model
          .get([
            new MetadataRequest(
                entryA, ['size', 'modificationTime', 'contentThumbnailUrl']),
            new MetadataRequest(
                entryB, ['size', 'modificationTime', 'contentThumbnailUrl']),
          ])
          .then(results => {
            assertEquals(2, results.length);
            assertEquals(
                new Date(2015, 0, 1).toString(),
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                results[0].modificationTime.toString());
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(1024, results[0].size);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals('THUMBNAIL_URL_A', results[0].contentThumbnailUrl);
            assertEquals(
                new Date(2015, 1, 2).toString(),
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                results[1].modificationTime.toString());
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(2048, results[1].size);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals('THUMBNAIL_URL_B', results[1].contentThumbnailUrl);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testMultiMetadataProviderExternalAndContentProperty(callback) {
  const model = new MultiMetadataProvider(
      // @ts-ignore: error TS2352: Conversion of type '{ get: (requests: any) =>
      // Promise<never[]>; }' to type 'FileSystemMetadataProvider' may be a
      // mistake because neither type sufficiently overlaps with the other. If
      // this was intentional, convert the expression to 'unknown' first.
      /** @type {!FileSystemMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      /** @type {!ExternalMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(2, requests.length);
          assertEquals('filesystem://B', requests[0].entry.toURL());
          assertEquals('filesystem://C', requests[1].entry.toURL());
          assertArrayEquals(['imageWidth', 'present'], requests[0].names);
          assertArrayEquals(['imageWidth', 'present'], requests[1].names);
          return Promise.resolve([
            {present: false, imageWidth: 200},
            {present: true, imageWidth: 400},
          ]);
        },
      }),
      /** @type {!ContentMetadataProvider} */ ({
        get: function(requests) {
          const results = {
            'filesystem://A': {imageWidth: 100},
            'filesystem://C': {imageWidth: 300},
          };
          assertEquals(1, requests.length);
          assertTrue(requests[0].entry.toURL() in results);
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'any' can't be used to index type '{
          // 'filesystem://A': { imageWidth: number; }; 'filesystem://C': {
          // imageWidth: number; }; }'.
          return Promise.resolve([results[requests[0].entry.toURL()]]);
        },
      }),
      /** @type {!DlpMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      volumeManager);

  reportPromise(
      model
          .get([
            new MetadataRequest(entryA, ['imageWidth']),
            new MetadataRequest(entryB, ['imageWidth']),
            new MetadataRequest(entryC, ['imageWidth']),
          ])
          .then(results => {
            assertEquals(3, results.length);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(100, results[0].imageWidth);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(200, results[1].imageWidth);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(300, results[2].imageWidth);
          }),
      callback);
}

/**
 * Tests that we only use ExternalMetadataProvider for a DocumentsProvider file.
 * @param {()=>void} callback
 */
export function testMultiMetadataProviderFileSystemAndExternalForDP(callback) {
  const model = new MultiMetadataProvider(
      // @ts-ignore: error TS2352: Conversion of type '{ get: (requests: any) =>
      // Promise<never[]>; }' to type 'FileSystemMetadataProvider' may be a
      // mistake because neither type sufficiently overlaps with the other. If
      // this was intentional, convert the expression to 'unknown' first.
      /** @type {!FileSystemMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      /** @type {!ExternalMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(1, requests.length);
          assertEquals('filesystem://D', requests[0].entry.toURL());
          assertArrayEquals(
              [
                'canCopy',
                'canDelete',
                'canRename',
                'canAddChildren',
                'modificationTime',
                'size',
              ],
              requests[0].names);
          return Promise.resolve([
            {
              canCopy: true,
              canDelete: true,
              canRename: true,
              canAddChildren: true,
              size: 110,
              modificationTime: new Date(2015, 1, 2),
            },
          ]);
        },
      }),
      // @ts-ignore: error TS2352: Conversion of type '{ get: (requests: any) =>
      // Promise<never[]>; }' to type 'ContentMetadataProvider' may be a mistake
      // because neither type sufficiently overlaps with the other. If this was
      // intentional, convert the expression to 'unknown' first.
      /** @type {!ContentMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      /** @type {!DlpMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      volumeManager);

  reportPromise(
      model
          .get([
            new MetadataRequest(
                entryD,
                [
                  'size',
                  'canCopy',
                  'canDelete',
                  'canRename',
                  'canAddChildren',
                ]),
          ])
          .then(results => {
            assertEquals(1, results.length);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(110, results[0].size);
            assertEquals(
                new Date(2015, 1, 2).toString(),
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                results[0].modificationTime.toString());
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(true, results[0].canCopy);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(true, results[0].canDelete);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(true, results[0].canRename);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(true, results[0].canAddChildren);
          }),
      callback);
}

/** @param {()=>void} callback */
export function testDlpMetadataProvider(callback) {
  const model = new MultiMetadataProvider(
      // @ts-ignore: error TS2352: Conversion of type '{ get: (requests: any) =>
      // Promise<never[]>; }' to type 'FileSystemMetadataProvider' may be a
      // mistake because neither type sufficiently overlaps with the other. If
      // this was intentional, convert the expression to 'unknown' first.
      /** @type {!FileSystemMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      /** @type {!ExternalMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      // @ts-ignore: error TS2352: Conversion of type '{ get: (requests: any) =>
      // Promise<never[]>; }' to type 'ContentMetadataProvider' may be a mistake
      // because neither type sufficiently overlaps with the other. If this was
      // intentional, convert the expression to 'unknown' first.
      /** @type {!ContentMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      }),
      /** @type {!DlpMetadataProvider} */ ({
        get: function(requests) {
          assertEquals(1, requests.length);
          return Promise.resolve([{
            sourceUrl: 'url',
            isDlpRestricted: true,
          }]);
        },
      }),
      volumeManager);

  reportPromise(
      model
          .get([
            new MetadataRequest(entryA, ['sourceUrl', 'isDlpRestricted']),
          ])
          .then(results => {
            assertEquals(1, results.length);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals('url', results[0].sourceUrl);
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            assertEquals(true, results[0].isDlpRestricted);
          }),
      callback);
}
