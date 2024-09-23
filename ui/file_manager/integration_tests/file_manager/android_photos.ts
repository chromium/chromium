// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

export type ElementQuery = string|string[];

export async function androidPhotosBanner() {
  // Add test files.
  // Photos provider currently does not have subdirectories, but we need one
  // there to tell that it's mounted and clickable (has-children="true"
  // selector).
  await addEntries(
      ['photos_documents_provider'], [ENTRIES.photos, ENTRIES.image2]);
  await addEntries(['local'], [ENTRIES.hello]);

  // Open Files app.
  const appId = await remoteCall.openNewWindow(RootPath.DOWNLOADS);

  const click = async (query: ElementQuery) => {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [query]),
        'fakeMouseClick failed');
  };
  const waitForElement = async (query: ElementQuery) => {
    await remoteCall.waitForElement(appId, query);
  };
  const waitForElementLost = async (query: ElementQuery) => {
    await remoteCall.waitForElementLost(appId, query);
  };
  const waitForFile = async (name: ElementQuery) => {
    await remoteCall.waitForElement(appId, `#file-list [file-name="${name}"]`);
  };

  await remoteCall.isolateBannerForTesting(appId, 'photos-welcome-banner');
  const photosBannerHiddenQuery = '#banners > photos-welcome-banner[hidden]';
  const photosBannerShownQuery =
      '#banners > photos-welcome-banner:not([hidden])';
  const photosBannerTextQuery = [
    '#banners > photos-welcome-banner',
    'educational-banner',
    '#educational-text-group',
  ];
  const photosBannerDismissButton = [
    '#banners > photos-welcome-banner',
    'educational-banner',
    await remoteCall.isCrosComponents(appId) ? '#dismiss-button' :
                                               '#dismiss-button-old',
  ];

  // Initial state: In the new framework banner is lazily loaded so will not be
  // attached to the DOM, without the banners framework the root element should
  // exist but the text should not be attached yet.
  await waitForElementLost(photosBannerTextQuery);

  // Wait for the DocumentsProvider volume to mount and navigate to Photos.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const photosVolumeType = 'documents_provider';
  await directoryTree.waitForItemToHaveChildrenByType(
      photosVolumeType, /* hasChildren= */ true);
  await directoryTree.selectItemByType(photosVolumeType);
  // Banner should be created and made visible.
  await waitForElement(photosBannerShownQuery);
  await waitForElement(photosBannerTextQuery);

  // Banner should disappear when navigating away (child elements are still in
  // DOM).
  await directoryTree.selectItemByLabel('Downloads');
  await waitForElement(photosBannerHiddenQuery);
  await waitForElement(photosBannerTextQuery);

  // Banner should re-appear when navigating to Photos again.
  await directoryTree.selectItemByType(photosVolumeType);
  await waitForElement(photosBannerShownQuery);

  // Dismiss the banner (created banner still in DOM).
  await waitForElement(photosBannerDismissButton);
  await click(photosBannerDismissButton);
  await waitForElement(photosBannerHiddenQuery);

  // Navigate away and then back, it should not re-appear.
  await directoryTree.selectItemByLabel('Downloads');
  await waitForFile('hello.txt');
  await directoryTree.selectItemByType(photosVolumeType);
  await waitForFile('image2.png');
  await waitForElement(photosBannerHiddenQuery);
}
