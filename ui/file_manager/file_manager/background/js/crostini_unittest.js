// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
};

/** @type {!VolumeManagerCommon.RootType<string>} */
let volumeManagerRootType;

/** @type {!VolumeManager} */
let volumeManager;

/** @type {!Crostini} */
let crostini;

// Set up the test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData = {
    data: {},
    getBoolean: function(key) {
      return window.loadTimeData.data[key];
    },
    getString: id => id,
  };

  // Create a fake volume manager that provides entry location info.
  volumeManager = /** @type {!VolumeManager} */ ({
    getLocationInfo: (entry) => {
      return /** @type {!EntryLocation} */ ({
        rootType: volumeManagerRootType,
      });
    },
  });

  // Reset initial root type.
  volumeManagerRootType =
      /** @type {!VolumeManagerCommon.RootType<string>} */ ('testroot');

  // Create and initialize Crostini.
  crostini = createCrostiniForTest();
  crostini.initVolumeManager(volumeManager);
}

/**
 * Tests init sets crostini and PluginVm enabled status.
 */
function testInitCrostiniPluginVmEnabled() {
  window.loadTimeData.data['CROSTINI_ENABLED'] = true;
  window.loadTimeData.data['PLUGIN_VM_ENABLED'] = true;
  crostini.initEnabled();
  assertTrue(crostini.isEnabled('termina'));
  assertTrue(crostini.isEnabled('PvmDefault'));

  window.loadTimeData.data['CROSTINI_ENABLED'] = false;
  window.loadTimeData.data['PLUGIN_VM_ENABLED'] = false;
  crostini.initEnabled();
  assertFalse(crostini.isEnabled('termina'));
  assertFalse(crostini.isEnabled('PvmDefault'));
}

/**
 * Tests path sharing.
 */
function testIsPathShared() {
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
function testCanSharePath() {
  crostini.setEnabled('vm', true);

  const mockFileSystem = new MockFileSystem('test');
  const root = MockDirectoryEntry.create(mockFileSystem, '/');
  const rootFile = new MockEntry(mockFileSystem, '/file');
  const rootFolder = MockDirectoryEntry.create(mockFileSystem, '/folder');
  const fooFile = new MockEntry(mockFileSystem, '/foo/file');
  const fooFolder = MockDirectoryEntry.create(mockFileSystem, '/foo/folder');

  // TODO(crbug.com/917920): Add computers_grand_root and computers when DriveFS
  // enforces allowed write paths.

  const allowed = [
    'downloads', 'removable', 'android_files', 'drive',
    'shared_drives_grand_root', 'team_drive'
  ];
  for (const type of allowed) {
    volumeManagerRootType = type;
    // TODO(crbug.com/958840): Sharing Play files root is disallowed until
    // we can ensure it will not also share Downloads.
    if (type === 'android_files') {
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

  // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
  const grandRootFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers');
  const computerRootFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers/My');
  const computerFolder =
      MockDirectoryEntry.create(mockFileSystem, '/Computers/My/foo');
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerFolder, false));
  volumeManagerRootType = VolumeManagerCommon.RootType.COMPUTER;
  assertFalse(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('vm', grandRootFolder, false));
  assertFalse(crostini.canSharePath('vm', computerRootFolder, false));
  assertTrue(crostini.canSharePath('vm', computerFolder, false));

  // Sharing LinuxFiles is allowed for all VMs except termina.
  volumeManagerRootType = VolumeManagerCommon.RootType.CROSTINI;
  assertTrue(crostini.canSharePath('vm', root, false));
  assertFalse(crostini.canSharePath('termina', root, false));
}
