// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';


/**
 * Tests the order is sorted correctly for each of the columns.
 */
export async function sortColumns() {
  const NAME_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.world,
  ]);

  const NAME_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.beautiful,
  ]);

  const SIZE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.desktop,
    ENTRIES.beautiful,
    ENTRIES.world,
  ]);

  const SIZE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.world,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.hello,
  ]);

  const TYPE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.world,
    ENTRIES.hello,
    ENTRIES.desktop,
  ]);

  const TYPE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.desktop,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.beautiful,
  ]);

  const DATE_ASC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.hello,
    ENTRIES.world,
    ENTRIES.desktop,
    ENTRIES.beautiful,
  ]);

  const DATE_DESC = TestEntryInfo.getExpectedRows([
    ENTRIES.photos,
    ENTRIES.beautiful,
    ENTRIES.desktop,
    ENTRIES.world,
    ENTRIES.hello,
  ]);

  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  const iconSortedAsc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_up_small"]';
  const iconSortedDesc =
      '.table-header-cell .sorted [iron-icon="files16:arrow_down_small"]';

  let a11yMessages;

  // Click the 'Name' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
  console.info(a11yMessages[0]);

  // Check: sort-button has aria-haspopup set to true
  const sortButton = await remoteCall.waitForElement(appId, '#sort-button');
  chrome.test.assertEq(sortButton.attributes['aria-haspopup'], 'true');

  // Click the 'Name' again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, iconSortedDesc);
  await remoteCall.waitForFiles(appId, NAME_DESC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');

  // Click the 'Size' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(2)']);
  await remoteCall.waitForElement(appId, iconSortedDesc);
  await remoteCall.waitForFiles(appId, SIZE_DESC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(3, a11yMessages.length, 'Missing a11y message');

  // 'Size' should be checked in the sort menu.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-size[checked]');

  // Click the 'Size' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(2)']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, SIZE_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(4, a11yMessages.length, 'Missing a11y message');

  // 'Size' should still be checked in the sort menu, even when the sort order
  // is reversed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-size[checked]');

  // Click the 'Type' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(3)']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, TYPE_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(5, a11yMessages.length, 'Missing a11y message');

  // Click the 'Type' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(3)']);
  await remoteCall.waitForElement(appId, iconSortedDesc);
  await remoteCall.waitForFiles(appId, TYPE_DESC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(6, a11yMessages.length, 'Missing a11y message');

  // 'Type' should still be checked in the sort menu, even when the sort order
  // is reversed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-type[checked]');

  // Click the 'Date modified' column header and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  await remoteCall.waitForElement(appId, iconSortedDesc);
  await remoteCall.waitForFiles(appId, DATE_DESC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(7, a11yMessages.length, 'Missing a11y message');

  // Click the 'Date modified' column header again and check the list.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(4)']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, DATE_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(8, a11yMessages.length, 'Missing a11y message');

  // 'Date modified' should still be checked in the sort menu.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-button']);
  await remoteCall.waitForElement(appId, '#sort-menu-sort-by-date[checked]');

  // Click 'Name' in the sort menu and check the result.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-menu-sort-by-name']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(9, a11yMessages.length, 'Missing a11y message');

  // Click the 'Name' again to reverse the order (to descending order).
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['.table-header-cell:nth-of-type(1)']);
  await remoteCall.waitForElement(appId, iconSortedDesc);
  await remoteCall.waitForFiles(appId, NAME_DESC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(10, a11yMessages.length, 'Missing a11y message');

  // Click 'Name' in the sort menu again should get the order back to
  // ascending order.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['#sort-menu-sort-by-name']);
  await remoteCall.waitForElement(appId, iconSortedAsc);
  await remoteCall.waitForFiles(appId, NAME_ASC, {orderCheck: true});

  // Fetch A11y messages.
  a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', appId, []);
  chrome.test.assertEq(11, a11yMessages.length, 'Missing a11y message');
}
