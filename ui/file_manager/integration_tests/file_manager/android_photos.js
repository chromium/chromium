// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

testcase.androidPhotosBanner = async () => {
  // Add test files.
  // Photos provider currently does not have subdirectories, but we need one
  // there to tell that it's mounted and clickable (has-children="true"
  // selector).
  await addEntries(
      ['photos_documents_provider'], [ENTRIES.photos, ENTRIES.image2]);
  await addEntries(['local'], [ENTRIES.hello]);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  const click = async (query) => {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [query]),
        'fakeMouseClick failed');
  };
  const waitForElement = async (query) => {
    await remoteCall.waitForElement(appId, query);
  };
  const waitForElementLost = async (query) => {
    await remoteCall.waitForElementLost(appId, query);
  };
  const waitForFile = async (name) => {
    await remoteCall.waitForElement(appId, `#file-list [file-name="${name}"]`);
  };

  // Initial state: banner root element should be hidden and children not
  // constructed.
  await waitForElement('#photos-welcome[hidden]');
  await waitForElementLost('.photos-welcome-message');

  // Wait for the DocumentsProvider volume to mount and navigate to Photos.
  const photosVolumeQuery =
      '[has-children="true"] [volume-type-icon="documents_provider"]';
  await waitForElement(photosVolumeQuery);
  await click(photosVolumeQuery);
  // Banner should be created and made visible.
  await waitForElement('#photos-welcome:not(.photos-welcome-hidden)');
  await waitForElement('.photos-welcome-message');

  // Banner should disappear when navigating away (child elements are still in
  // DOM).
  await click('[volume-type-icon="downloads"]');
  await waitForElement('#photos-welcome.photos-welcome-hidden');
  await waitForElement('.photos-welcome-message');

  // Banner should re-appear when navigating to Photos again.
  await click(photosVolumeQuery);
  await waitForElement('#photos-welcome:not(.photos-welcome-hidden)');

  // Dismiss the banner (created banner still in DOM).
  await waitForElement('#photos-welcome-dismiss');
  await click('#photos-welcome-dismiss');
  await waitForElement('#photos-welcome.photos-welcome-hidden');

  // Navigate away and then back, it should not re-appear.
  await click('[volume-type-icon="downloads"]');
  await waitForFile('hello.txt');
  await click(photosVolumeQuery);
  await waitForFile('image2.png');
  await waitForElement('#photos-welcome.photos-welcome-hidden');
};
