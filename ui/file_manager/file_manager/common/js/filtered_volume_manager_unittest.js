// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeInfoList} from '../../externs/volume_info_list.js';

import {FilteredVolumeManager} from './filtered_volume_manager.js';
import {AllowedPaths, VolumeManagerCommon} from './volume_manager_types.js';

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
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager), [],
      []);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: hasDisabledVolumes() should return false.
    assertFalse(filteredVolumeManager.hasDisabledVolumes());

    // Check: getFuseBoxOnlyFilterEnabled should return false.
    assertFalse(filteredVolumeManager.getFuseBoxOnlyFilterEnabled());

    // Check: getMediaStoreFilesOnlyFilterEnabled should return false.
    assertFalse(filteredVolumeManager.getMediaStoreFilesOnlyFilterEnabled());

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
   * SelectFileAsh (Lacros) ['fusebox-only'] filter: FilteredVolumeManager
   * should only show native and fusebox volumes in the files app UI.
   */
  const filteredVolumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager),
      ['fusebox-only'], []);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: hasDisabledVolumes() should return false.
    assertFalse(filteredVolumeManager.hasDisabledVolumes());

    // Check: getFuseBoxOnlyFilterEnabled should return true.
    assertTrue(filteredVolumeManager.getFuseBoxOnlyFilterEnabled());

    // Check: getMediaStoreFilesOnlyFilterEnabled should return false.
    assertFalse(filteredVolumeManager.getMediaStoreFilesOnlyFilterEnabled());

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

/**
 * Tests FilteredVolumeManager 'media-store-files-only' volume filter.
 */
export function testVolumeMediaStoreFilesOnlyFilter(done) {
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

  // Add `ARCHIVE` volume.
  const zipArchiveVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'zipArchiveVolumeId',
      'zip archive volume', 'zip-archive-volume-path');
  volumeManager.volumeInfoList.add(zipArchiveVolumeInfo);

  // Add `REMOVABLE` volume.
  const removableVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removableVolumeId',
      'removable volume', 'removable-volume-path');
  volumeManager.volumeInfoList.add(removableVolumeInfo);

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

  // Add `PROVIDED` volume.
  const fspVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'fspNormalVolumeId',
      'FSP normal volume', 'fsp-provider-path');
  volumeManager.volumeInfoList.add(fspVolumeInfo);

  // Add `PROVIDED` fusebox volume.
  const fspFuseBoxVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'fspFuseBoxVolumeId',
      'FSP fusebox volume', 'fusebox/fsp-provider-path');
  volumeManager.volumeInfoList.add(fspFuseBoxVolumeInfo);

  // Add `SMB` volume.
  const smbVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.SMB, 'smbVolumeId',
      'SMB server message block volume', 'server-message-block-path');
  volumeManager.volumeInfoList.add(smbVolumeInfo);

  // Check: volumeManager.volumeInfoList should have 10 volumes.
  assertEquals(10, volumeManager.volumeInfoList.length);

  /**
   * ArcSelectFile ['media-store-files-only'] filter: FilteredVolumeManager
   * should only allow volumes that are indexed by the Android MediaStore.
   */
  const filteredVolumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager),
      ['media-store-files-only'], []);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: hasDisabledVolumes() should return false.
    assertFalse(filteredVolumeManager.hasDisabledVolumes());

    // Check: getFuseBoxOnlyFilterEnabled should return false.
    assertFalse(filteredVolumeManager.getFuseBoxOnlyFilterEnabled());

    // Check: getMediaStoreFilesOnlyFilterEnabled should return true.
    assertTrue(filteredVolumeManager.getMediaStoreFilesOnlyFilterEnabled());

    // Check: filteredVolumeManager.volumeInfoList should have 2 volumes.
    assertEquals(2, filteredVolumeManager.volumeInfoList.length);

    // Get `DOWNLOADS` volume.
    let info = filteredVolumeManager.volumeInfoList.item(0);
    assert(info, 'volume[0] DOWNLOADS expected');
    assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, info.volumeType);

    // Get `REMOVABLE` volume.
    info = filteredVolumeManager.volumeInfoList.item(1);
    assert(info, 'volume[1] REMOVABLE expected');
    assertEquals(VolumeManagerCommon.VolumeType.REMOVABLE, info.volumeType);

    done();
  });
}

/**
 * Tests the disabled volume related functions.
 */
export function testDisabledVolumes(done) {
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

  const filteredVolumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH_OR_URL, false, Promise.resolve(volumeManager), [],
      [VolumeManagerCommon.VolumeType.DRIVE]);

  filteredVolumeManager.ensureInitialized(() => {
    // Check: hasDisabledVolumes() should return true.
    assertTrue(filteredVolumeManager.hasDisabledVolumes());

    // Check: isDisabled() should return true for DRIVE.
    assertTrue(
        filteredVolumeManager.isDisabled(VolumeManagerCommon.VolumeType.DRIVE));

    // Check: isDisabled() should return false for REMOVABLE.
    assertFalse(filteredVolumeManager.isDisabled(
        VolumeManagerCommon.VolumeType.REMOVABLE));

    done();
  });
}
