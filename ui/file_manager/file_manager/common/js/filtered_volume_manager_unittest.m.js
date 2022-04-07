// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';

import {FilteredVolumeManager} from './filtered_volume_manager.js';
import {AllowedPaths, VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Setup the test components.
 */
export function setUp() {
  loadTimeData.getString = id => id;
  loadTimeData.resetForTesting({});
}

/**
 * Create a new MockVolumeManager for each test fixture.
 * @return {!MockVolumeManager}
 */
function createMockVolumeManager() {
  // Create mock volume manager.
  const volumeManager = new MockVolumeManager();

  // Patch its addEventListener, removeEventListener methods to make them log
  // (instead of throw) "not implemented", since those throw events break the
  // FilteredVolumeManager initialization code in tests.
  volumeManager.addEventListener = (type, handler) => {
    console.log('MockVolumeManager.addEventListener not implemented');
  };
  volumeManager.removeEventListener = (type, handler) => {
    console.log('MockVolumeManager.removeEventListener not implemented');
  };

  return volumeManager;
}

/**
 * Tests FilteredVolumeManager default volume filter.
 */
export function testVolumeDefaultFilter(done) {
  // Create mock volume manager.
  const volumeManager = createMockVolumeManager();

  // Get `DRIVE` volume.
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  assert(driveVolumeInfo);

  // Get `DOWNLOADS` volume.
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  assert(downloadsVolumeInfo);

  // Add `MTP` volume.
  const mtpVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtpNormalVolumeId',
      'MTP normal volume', 'mtp-path');
  volumeManager.volumeInfoList.add(mtpVolumeInfo);

  // Add `MTP` fusebox volume.
  const mtpFuseBoxVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtpFuseBoxVolumeId',
      'MTP fusebox volume', 'fusebox/mtp-path');
  volumeManager.volumeInfoList.add(mtpFuseBoxVolumeInfo);

  // Add `DOCUMENTS_PROVIDER` volume.
  const documentsProviderVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'adpNormalVolumeId',
      'Documents provider normal volume', 'documents-provider-path');
  volumeManager.volumeInfoList.add(documentsProviderVolumeInfo);

  // Check: volumeManager.volumeInfoList should have 5 volumes.
  assertEquals(5, volumeManager.volumeInfoList.length);

  /**
   * Default volume filter []: FilteredVolumeManager should remove fusebox
   * volumes from the files app UI.
   */
  const filteredVolumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager), []);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: filteredVolumeManager.volumeInfoList should have 4 volumes.
    assertEquals(4, filteredVolumeManager.volumeInfoList.length);

    // Get `DRIVE` volume.
    let info = filteredVolumeManager.volumeInfoList.item(0);
    assert(info, 'volume[0] DRIVE expected');
    assertEquals(VolumeManagerCommon.VolumeType.DRIVE, info.volumeType);

    // Get `DOWNLOADS` volume.
    info = filteredVolumeManager.volumeInfoList.item(1);
    assert(info, 'volume[1] DOWNLOADS expected');
    assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, info.volumeType);

    // Get `MTP` volume.
    info = filteredVolumeManager.volumeInfoList.item(2);
    assert(info, 'volume[2] MTP expected');
    assertEquals(VolumeManagerCommon.VolumeType.MTP, info.volumeType);

    // Check: the MTP volume should be a normal volume.
    assertEquals('MTP normal volume', info.label);
    assertEquals('mtp-path', info.devicePath);

    // Get `DOCUMENTS_PROVIDER` volume.
    info = filteredVolumeManager.volumeInfoList.item(3);
    assert(info, 'volume[3] DOCUMENTS_PROVIDER expected');
    assertEquals(
        VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, info.volumeType);

    // Check: the DOCUMENTS_PROVIDER volume should be a normal volume.
    assertEquals('Documents provider normal volume', info.label);
    assertEquals('documents-provider-path', info.devicePath);

    done();
  });
}

/**
 * Tests FilteredVolumeManager 'fusebox-only' volume filter.
 */
export function testVolumeFuseboxOnlyFilter(done) {
  // Create mock volume manager.
  const volumeManager = createMockVolumeManager();

  // Get `DRIVE` volume.
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  assert(driveVolumeInfo);

  // Get `DOWNLOADS` volume.
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  assert(downloadsVolumeInfo);

  // Add `MTP` volume.
  const mtpVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtpNormalVolumeId',
      'MTP normal volume', 'mtp-path');
  volumeManager.volumeInfoList.add(mtpVolumeInfo);

  // Add `MTP` fusebox volume.
  const mtpFuseBoxVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'mtpFuseBoxVolumeId',
      'MTP fusebox volume', 'fusebox/mtp-path');
  volumeManager.volumeInfoList.add(mtpFuseBoxVolumeInfo);

  // Add `DOCUMENTS_PROVIDER` volume.
  const documentsProviderVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'adpNormalVolumeId',
      'Documents provider normal volume', 'documents-provider-path');
  volumeManager.volumeInfoList.add(documentsProviderVolumeInfo);

  // Check: volumeManager.volumeInfoList should have 5 volumes.
  assertEquals(5, volumeManager.volumeInfoList.length);

  /**
   * SelectFileAsh sets ['fusebox-only'] filter: FilteredVolumeManager should
   * only show native and fusebox volumes in the files app UI.
   */
  const filteredVolumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager),
      ['fusebox-only']);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: filteredVolumeManager.volumeInfoList should have 3 volumes.
    assertEquals(3, filteredVolumeManager.volumeInfoList.length);

    // Get `DRIVE` volume.
    let info = filteredVolumeManager.volumeInfoList.item(0);
    assert(info, 'volume[0] DRIVE expected');
    assertEquals(VolumeManagerCommon.VolumeType.DRIVE, info.volumeType);

    // Get `DOWNLOADS` volume.
    info = filteredVolumeManager.volumeInfoList.item(1);
    assert(info, 'volume[1] DOWNLOADS expected');
    assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, info.volumeType);

    // Get `MTP` volume.
    info = filteredVolumeManager.volumeInfoList.item(2);
    assert(info, 'volume[2] MTP expected');
    assertEquals(VolumeManagerCommon.VolumeType.MTP, info.volumeType);

    // Check: the MTP volume should be a fusebox volume.
    assertEquals('MTP fusebox volume', info.label);
    assertEquals('fusebox/mtp-path', info.devicePath);
    assertEquals('fusebox', info.diskFileSystemType);

    done();
  });
}
