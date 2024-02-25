// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {assertArrayEquals, assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {VolumeType} from '../../../common/js/volume_manager_types.js';

import type {ContentMetadataProvider} from './content_metadata_provider.js';
import type {DlpMetadataProvider} from './dlp_metadata_provider.js';
import type {ExternalMetadataProvider} from './external_metadata_provider.js';
import type {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import type {MetadataItem} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';
import {MultiMetadataProvider} from './multi_metadata_provider.js';

const entryA = {
  toURL: function() {
    return 'filesystem://A';
  },
} as Entry;

const entryB = {
  toURL: function() {
    return 'filesystem://B';
  },
} as Entry;

const entryC = {
  toURL: function() {
    return 'filesystem://C';
  },
} as Entry;

const entryD = {
  toURL: function() {
    return 'filesystem://D';
  },
} as Entry;

const volumeManager = {
  getVolumeInfo: function(entry) {
    if (entry.toURL() === 'filesystem://A') {
      return {
        volumeType: VolumeType.DOWNLOADS,
      };
    } else if (entry.toURL() === 'filesystem://B') {
      return {
        volumeType: VolumeType.DRIVE,
      };
    } else if (entry.toURL() === 'filesystem://C') {
      return {
        volumeType: VolumeType.DRIVE,
      };
    } else if (entry.toURL() === 'filesystem://D') {
      return {
        volumeType: VolumeType.DOCUMENTS_PROVIDER,
      };
    }
    assertNotReached();
  },
} as VolumeManager;

export async function testMultiMetadataProviderBasic() {
  const model = new MultiMetadataProvider(
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(1, requests.length);
          const request0 = requests[0]!;
          assertEquals('filesystem://A', request0.entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], request0.names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 0, 1), size: 1024}]);
        },
      } as FileSystemMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(1, requests.length);
          assertEquals('filesystem://B', requests[0]!.entry.toURL());
          assertArrayEquals(['size', 'modificationTime'], requests[0]!.names);
          return Promise.resolve(
              [{modificationTime: new Date(2015, 1, 2), size: 2048}]);
        },
      } as ExternalMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          if (requests.length === 0) {
            return Promise.resolve([]);
          }
          assertEquals(2, requests.length);
          assertEquals('filesystem://A', requests[0]!.entry.toURL());
          assertEquals('filesystem://B', requests[1]!.entry.toURL());
          assertArrayEquals(['contentThumbnailUrl'], requests[0]!.names);
          assertArrayEquals(['contentThumbnailUrl'], requests[1]!.names);
          return Promise.resolve([
            {contentThumbnailUrl: 'THUMBNAIL_URL_A'},
            {contentThumbnailUrl: 'THUMBNAIL_URL_B'},
          ]);
        },
      } as ContentMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as DlpMetadataProvider,
      volumeManager);

  const results = await model.get([
    new MetadataRequest(
        entryA, ['size', 'modificationTime', 'contentThumbnailUrl']),
    new MetadataRequest(
        entryB, ['size', 'modificationTime', 'contentThumbnailUrl']),
  ]);
  assertEquals(2, results.length);
  assertEquals(
      new Date(2015, 0, 1).toString(),
      results[0]!.modificationTime?.toString());
  assertEquals(1024, results[0]!.size);
  assertEquals('THUMBNAIL_URL_A', results[0]!.contentThumbnailUrl);
  assertEquals(
      new Date(2015, 1, 2).toString(),
      results[1]!.modificationTime?.toString());
  assertEquals(2048, results[1]!.size);
  assertEquals('THUMBNAIL_URL_B', results[1]!.contentThumbnailUrl);
}

export async function testMultiMetadataProviderExternalAndContentProperty() {
  const model = new MultiMetadataProvider(
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as FileSystemMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(2, requests.length);
          assertEquals('filesystem://B', requests[0]!.entry.toURL());
          assertEquals('filesystem://C', requests[1]!.entry.toURL());
          assertArrayEquals(['imageWidth', 'present'], requests[0]!.names);
          assertArrayEquals(['imageWidth', 'present'], requests[1]!.names);
          return Promise.resolve([
            {present: false, imageWidth: 200},
            {present: true, imageWidth: 400},
          ]);
        },
      } as ExternalMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          const results: {[key: string]: MetadataItem} = {
            'filesystem://A': {imageWidth: 100},
            'filesystem://C': {imageWidth: 300},
          };
          assertEquals(1, requests.length);
          assertTrue(requests[0]!.entry.toURL() in results);
          return Promise.resolve([results[requests[0]!.entry.toURL()]!]);
        },
      } as ContentMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as DlpMetadataProvider,
      volumeManager);

  const results = await model.get([
    new MetadataRequest(entryA, ['imageWidth']),
    new MetadataRequest(entryB, ['imageWidth']),
    new MetadataRequest(entryC, ['imageWidth']),
  ]);
  assertEquals(3, results.length);
  assertEquals(100, results[0]!.imageWidth);
  assertEquals(200, results[1]!.imageWidth);
  assertEquals(300, results[2]!.imageWidth);
}

/**
 * Tests that we only use ExternalMetadataProvider for a DocumentsProvider file.
 */
export async function testMultiMetadataProviderFileSystemAndExternalForDP() {
  const model = new MultiMetadataProvider(
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as FileSystemMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(1, requests.length);
          assertEquals('filesystem://D', requests[0]!.entry.toURL());
          assertArrayEquals(
              [
                'canCopy',
                'canDelete',
                'canRename',
                'canAddChildren',
                'modificationTime',
                'size',
              ],
              requests[0]!.names);
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
      } as ExternalMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as ContentMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as DlpMetadataProvider,
      volumeManager);

  const results = await model.get([
    new MetadataRequest(
        entryD,
        [
          'size',
          'canCopy',
          'canDelete',
          'canRename',
          'canAddChildren',
        ]),
  ]);
  assertEquals(1, results.length);
  assertEquals(110, results[0]!.size);
  assertEquals(
      new Date(2015, 1, 2).toString(),
      results[0]!.modificationTime?.toString());
  assertEquals(true, results[0]!.canCopy);
  assertEquals(true, results[0]!.canDelete);
  assertEquals(true, results[0]!.canRename);
  assertEquals(true, results[0]!.canAddChildren);
}

export async function testDlpMetadataProvider() {
  const model = new MultiMetadataProvider(
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as FileSystemMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as ExternalMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(0, requests.length);
          return Promise.resolve([]);
        },
      } as ContentMetadataProvider,
      {
        get: function(requests: MetadataRequest[]): Promise<MetadataItem[]> {
          assertEquals(1, requests.length);
          return Promise.resolve([{
            sourceUrl: 'url',
            isDlpRestricted: true,
          }]);
        },
      } as DlpMetadataProvider,
      volumeManager);

  const results = await model.get([
    new MetadataRequest(entryA, ['sourceUrl', 'isDlpRestricted']),
  ]);
  assertEquals(1, results.length);
  assertEquals('url', results[0]!.sourceUrl);
  assertEquals(true, results[0]!.isDlpRestricted);
}
