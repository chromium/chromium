// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var dialogSettings = {};

function mountFileSystem(onSuccess, onError) {
  chrome.fileSystemProvider.getAll(function(mounted) {
    var index = mounted.length + 1;
    chrome.fileSystemProvider.mount({
      fileSystemId: 'test-fs-' + index,
      displayName: 'Test (' + index + ')'
    });
  });
}

chrome.fileSystemProvider.onGetMetadataRequested.addListener(
    function(options, onSuccess, onError) {
      onSuccess({
        isDirectory: true,
        name: '',
        size: 0,
        modificationTime: new Date()
      });
    });

chrome.fileSystemProvider.onReadDirectoryRequested.addListener(function(
    options, onSuccess, onError) {
  // For anything other than root, return no entries.
  if (options.directoryPath !== '/') {
    onSuccess([], false /* hasMore */);
    return;
  }
  // For root we return 1 folder entry.
  const entries = [
    {
      isDirectory: true,
      name: 'folder',
    },
  ];
  onSuccess(entries, false /* hasMore */);
});

chrome.fileSystemProvider.onMountRequested.addListener(mountFileSystem);

chrome.fileSystemProvider.onUnmountRequested.addListener(function(
    options, onSuccess, onError) {
  chrome.fileSystemProvider.unmount(
      {fileSystemId: options.fileSystemId}, function() {
        if (chrome.runtime.lastError) {
          onError(chrome.runtime.lastError.message);
        } else {
          onSuccess();
        }
      });
});

chrome.fileSystemProvider.onGetActionsRequested.addListener(
    function(options, onSuccess, onError) {
      onSuccess([]);
    });

// If the manifest for device or file source is used, then mount a fake file
// system on install.
if (chrome.runtime.getManifest().description === "Testing Provider device" ||
    chrome.runtime.getManifest().description === "Testing Provider file") {
  chrome.runtime.onInstalled.addListener(mountFileSystem);
}
