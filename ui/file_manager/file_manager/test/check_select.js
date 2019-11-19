// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const checkselect = {};

checkselect.testCancelCheckSelectModeAfterAction = async (done) => {
  await test.setupAndWaitUntilReady();

  // Click 2nd last file on checkmark to start check-select-mode.
  assertTrue(test.fakeMouseClick(
      '#file-list li.table-row:nth-of-type(4) .detail-checkmark'));
  await test.waitForElement('#file-list li[selected].table-row:nth-of-type(4)');

  // Click last file on checkmark, adds to selection.
  assertTrue(test.fakeMouseClick(
      '#file-list li.table-row:nth-of-type(5) .detail-checkmark'));
  await test.waitForElement('#file-list li[selected].table-row:nth-of-type(5)');
  assertEquals(2, document.querySelectorAll('#file-list li[selected]').length);

  // Click selection menu (3-dots).
  assertTrue(test.fakeMouseClick('#selection-menu-button'));
  await test.waitForElement(
      '#file-context-menu:not([hidden]) ' +
      'cr-menu-item[command="#cut"]:not([disabled])');

  // Click 'cut'.
  test.fakeMouseClick('#file-context-menu cr-menu-item[command="#cut"]');
  await test.waitForElement('#file-context-menu[hidden]');

  // Click first photos dir in checkmark and make sure 4 and 5 not
  // selected.
  assertTrue(test.fakeMouseClick(
      '#file-list li.table-row:nth-of-type(1) .detail-checkmark'));
  await test.waitForElement('#file-list li[selected].table-row:nth-of-type(1)');

  assertEquals(1, document.querySelectorAll('#file-list li[selected]').length);
  done();
};

checkselect.testCheckSelectModeAfterSelectAllOneFile = async (done) => {
  const gearMenu = document.querySelector('#gear-menu');
  const cancel = document.querySelector('#cancel-selection-button-wrapper');
  const selectAll =
      '#gear-menu:not([hidden]) #gear-menu-select-all:not([disabled])';

  // Load a single file.
  await test.setupAndWaitUntilReady([test.ENTRIES.hello]);
  // Click gear menu, ensure 'Select all' is shown.
  assertTrue(test.fakeMouseClick('#gear-button'));
  await test.waitForElement(selectAll);

  // Click 'Select all', gear menu now replaced with file context menu.
  assertTrue(test.fakeMouseClick('#gear-menu-select-all'));
  await test.repeatUntil(() => {
    return getComputedStyle(gearMenu).opacity == 0 &&
        getComputedStyle(cancel).display == 'block' ||
        test.pending('waiting for check select mode from click');
  });

  // Cancel selection, ensure no items selected.
  assertTrue(test.fakeMouseClick('#cancel-selection-button'));
  await test.repeatUntil(() => {
    return document.querySelectorAll('#file-list li[selected]').length == 0 ||
        test.pending('waiting for no files selected after click');
  });

  // 'Ctrl+a' to select all.
  assertTrue(test.fakeKeyDown('#file-list', 'a', true, false, false));
  await test.repeatUntil(() => {
    return getComputedStyle(cancel).display == 'block' ||
        test.pending('waiting for check select mode from key');
  });

  // Cancel selection, ensure no items selected.
  assertTrue(test.fakeMouseClick('#cancel-selection-button'));
  await test.repeatUntil(() => {
    return document.querySelectorAll('#file-list li[selected]').length == 0 ||
        test.pending('waiting for no files selected after key');
  });

  done();
};
