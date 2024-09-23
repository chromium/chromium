// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome, MockMetrics} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {RootType} from '../../common/js/volume_manager_types.js';

import {Crostini} from './crostini.js';
import type {EntryLocation} from './entry_location_impl.js';
import type {VolumeManager} from './volume_manager.js';

let volumeManagerRootType: RootType;

let volumeManager: VolumeManager;

let crostini: Crostini;

// Set up the test components.
export function setUp() {
  // Mock fileManagerPrivate.onCrostiniChanged.
  const mockChrome = {
    metricsPrivate: new MockMetrics(),
    fileManagerPrivate: {
      onCrostiniChanged: {
        addListener: () => {},
      },
    },
  };
  installMockChrome(mockChrome);

  // Create a fake volume manager that provides entry location info.
  volumeManager = {
    getLocationInfo: (_: Entry) => {
      return {
        rootType: volumeManagerRootType,
      } as EntryLocation;
    },
  } as VolumeManager;

  // Reset initial root type.
  volumeManagerRootType = 'testroot' as RootType;

  // Create and initialize Crostini.
  crostini = new Crostini();
  crostini.initVolumeManager(volumeManager);
}

/**
 * Tests init sets crostini and PluginVm enabled status.
 */
export function testInitCrostiniPluginVmEnabled() {
  loadTimeData.overrideValues({'VMS_FOR_SHARING': []});
  crostini.initEnabled();
  assertFalse(crostini.isEnabled('termina'));
  assertFalse(crostini.isEnabled('PvmDefault'));

  loadTimeData.overrideValues({
    'VMS_FOR_SHARING': [
      {'vmName': 'termina', 'containerName': 'penguin'},
      {'vmName': 'PvmDefault', 'containerName': ''},
    ],
  });
  crostini.initEnabled();
  assertTrue(crostini.isEnabled('termina'));
  assertTrue(crostini.isEnabled('PvmDefault'));
}

/**
 * Tests setEnabled tracks enabled/disabled correctly, in particular with
 * multiple containers in a VM.
 */
export function testSetEnabled() {
  assertFalse(crostini.isEnabled('termina'));

  crostini.setEnabled('termina', 'penguin', true);
  assertTrue(crostini.isEnabled('termina'));

  crostini.setEnabled('termina', 'puffin', true);
  assertTrue(crostini.isEnabled('termina'));

  crostini.setEnabled('termina', 'penguin', false);
  assertTrue(crostini.isEnabled('termina'));

  crostini.setEnabled('termina', 'puffin', false);
  assertFalse(crostini.isEnabled('termina'));
}

/**
 * Tests path sharing.
 */
export function testIsPathShared() {
  const mockFileSystem = new MockFileSystem('volumeId');
  const root = MockDirectoryEntry.create(mockFileSystem, '/');
  const a = MockDirectoryEntry.create(mockFileSystem, '/a');
  const aa = MockDirectoryEntry.create(mockFileSystem, '/a/a');
  const ab = MockDirectoryEntry.create(mockFileSystem, '/a/b');
  const b = MockDirectoryEntry.create(mockFileSystem, '/b');
  const bb = MockDirectoryEntry.create(mockFileSystem, '/b/b');

  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm2', a));

  crostini.registerSharedPath('vm1', a);
  assertFalse(crostini.isPathShared('vm1', root));
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));

  crostini.registerSharedPath('vm2', a);
  assertTrue(crostini.isPathShared('vm2', a));

  crostini.registerSharedPath('vm1', bb);
  assertFalse(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm1', bb));

  crostini.unregisterSharedPath('vm1', bb);
  assertFalse(crostini.isPathShared('vm1', bb));

  // Test collapsing vm1, but not vm2.  Setup with /a/a, /a/b, /b
  crostini.unregisterSharedPath('vm1', a);
  crostini.unregisterSharedPath('vm2', a);
  crostini.registerSharedPath('vm1', aa);
  crostini.registerSharedPath('vm1', ab);
  crostini.registerSharedPath('vm1', b);
  crostini.registerSharedPath('vm2', aa);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm2', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  // Add /a for vm1, collapses /a/a, /a/b in vm1.
  crostini.registerSharedPath('vm1', a);
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  assertFalse(crostini.isPathShared('vm2', a));
  // Unregister /a for vm1, /a/a and /a/b should be lost in vm1.
  crostini.unregisterSharedPath('vm1', a);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm1', aa));
  assertFalse(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
  // Register root for vm1, collapses all vm1.
  crostini.registerSharedPath('vm1', root);
  assertTrue(crostini.isPathShared('vm1', a));
  assertTrue(crostini.isPathShared('vm1', aa));
  assertTrue(crostini.isPathShared('vm1', ab));
  assertTrue(crostini.isPathShared('vm1', b));
  // Unregister root for vm1, all vm1 should be lost.
  crostini.unregisterSharedPath('vm1', root);
  assertFalse(crostini.isPathShared('vm1', a));
  assertFalse(crostini.isPathShared('vm1', aa));
  assertFalse(crostini.isPathShared('vm1', ab));
  assertFalse(crostini.isPathShared('vm1', b));
  assertTrue(crostini.isPathShared('vm2', aa));
}

/*
 * Tests disallowed and allowed shared paths.
 */
export function testCanSharePath() {
  crostini.setEnabled('vm', '', true);

  const mockFileSystem = new MockFileSystem('test');
  const root = MockDirectoryEntry.create(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file') as any as Entry;
  const rootFolder = MockDirectoryEntry.create(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file') as any as Entry;
  const fooFolder = MockDirectoryEntry.create(mockFileSystem, '/foo/folder');

  // TODO(crbug.com/40607763): Add computers_grand_root and computers when DriveFS
  // enforces allowed write paths.

  const allowed = [
    RootType.DOWNLOADS,
    RootType.REMOVABLE,
    RootType.ANDROID_FILES,
    RootType.DRIVE,
    RootType.SHARED_DRIVES_GRAND_ROOT,
    RootType.SHARED_DRIVE,
    RootType.DRIVE_SHARED_WITH_ME,
  ];
  for (const type of allowed) {
    volumeManagerRootType = type;
    // TODO(crbug.com/41456343): Sharing Play files root is disallowed until
    // we can ensure it will not also share Downloads.
    // We don't share 'Shared with me' root since it is fake.
    if ([
          RootType.ANDROID_FILES,
          RootType.DRIVE_SHARED_WITH_ME,
        ].includes(type)) {
      assertFalse(crostini.canSharePath('vm', root, true));
      assertFalse(crostini.canSharePath('vm', root, false));
    } else {
      assertTrue(crostini.canSharePath('vm', root, true));
      assertTrue(crostini.canSharePath('vm', root, false));
    }
    assertFalse(crostini.canSharePath('vm', rootFile, true));
    assertTrue(crostini.canSharePath('vm', rootFile, false));
    assertTrue(crostini.canSharePath('vm', rootFolder, true));
    assertTrue(crostini.canSharePath('vm', rootFolder, false));
    assertFalse(crostini.canSharePath('vm', fooFile, true));
    assertTrue(crostini.canSharePath('vm', fooFile, false));
    assertTrue(crostini.canSharePath('vm', fooFolder, true));
    assertTrue(crostini.canSharePath('vm', fooFolder, false));
  }

  // TODO(crbug.com/40607763): Remove when DriveFS enforces allowed write paths.
  const grandRootFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers');
  const computerRootFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers/My');
  const computerFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers/My/foo');
  volumeManagerRootType = RootType.COMPUTERS_GRAND_ROOT;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerFolder, false));
  volumeManagerRootType = RootType.COMPUTER;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertTrue(crostini.canSharePath('vm', computerFolder, false));

  // Sharing LinuxFiles is allowed for all VMs except termina.
  volumeManagerRootType = RootType.CROSTINI;
  assertTrue(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('termina', root, false));
}
