// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pluginVm = {};

pluginVm.testLabelIconContextMenu = async (done) => {
  const fileMenu = [
    ['#cut', false],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#get-info', true],
    ['#delete', false],
    ['#zip-selection', true],
    ['#share-with-linux', true],
    ['#new-folder', true],
  ];

  const fileMenuSubfolder = [
    ['#cut', true],
    ['#copy', true],
    ['#paste-into-folder', false],
    ['#get-info', true],
    ['#rename', true],
    ['#delete', true],
    ['#zip-selection', true],
    ['#share-with-linux', true],
    ['#new-folder', true],
  ];

  const pluginVmFolder = '#file-list [file-name="PvmDefault"]';
  const photosSubfolder = '#file-list [file-name="photos"]';
  const iconFolder =
      '#file-list [file-name="PvmDefault"] [file-type-icon="plugin_vm"]';
  const fileMenuShown = '#file-context-menu:not([hidden])';

  const iconDirTree = '#directory-tree [file-type-icon="plugin_vm"]';
  const dirTreeMenuShown = '#directory-tree-context-menu:not([hidden])';
  const itemsShown = ' cr-menu-item:not([hidden])';

  async function waitForMenuItems(rightClick, menuTypeShown, expectedItems) {
    const expected = JSON.stringify(expectedItems);
    return test.repeatUntil(() => {
      assertTrue(test.fakeMouseRightClick(rightClick));
      const items =
          Array.from(document.querySelectorAll(menuTypeShown + itemsShown))
              .map(e => [e.attributes['command'].value, !e.disabled]);
      const actual = JSON.stringify(items);
      return expected === actual ||
          test.pending(
              'Waiting for context menu, expected: %s, actual: %s', expected,
              actual);
    });
  }

  // Verify that /PvmDefault has label 'Plugin VM'.
  await test.setupAndWaitUntilReady([], [], []);
  test.addEntries(
      [test.ENTRIES.pluginVm, test.ENTRIES.photosInPluginVm], [], []);
  test.refreshFileList();
  await test.waitForFiles(test.TestEntryInfo.getExpectedRows(
      [test.ENTRIES.pluginVm, test.ENTRIES.linuxFiles]));

  // Verify folder icon.
  await test.waitForElement(iconFolder);

  // Verify /PvmDefault folder context menu.
  await waitForMenuItems(pluginVmFolder, fileMenuShown, fileMenu);

  // Change to 'PvmDefault' directory, photos folder is shown.
  assertTrue(test.fakeMouseDoubleClick(pluginVmFolder));
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows([test.ENTRIES.photos]));

  // Verify /PvmDefault/photos folder context menu.
  await waitForMenuItems(photosSubfolder, fileMenuShown, fileMenuSubfolder);

  done();
};
