// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

// Mock items.
var progressCenter = null;

// Test target.
var handler = null;

/**
 * Mock of chrome.fileManagerPrivate.
 * @type {Object}
 * @const
 */
chrome.fileManagerPrivate = {
  onFileTransfersUpdated: {
    addListener: function(callback) {
      chrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = callback;
    },
    removeListener: function() {
      chrome.fileManagerPrivate.onFileTransfersUpdated.listener_ = null;
    },
    listener_: null
  },
  onDriveSyncError: {
    addListener: function(callback) {
      chrome.fileManagerPrivate.onDriveSyncError.listener_ = callback;
    },
    removeListener: function() {
      chrome.fileManagerPrivate.onDriveSyncError.listener_ = null;
    },
    listener_: null
  },
  onPreferencesChanged: {
    addListener: function(callback) {
      chrome.fileManagerPrivate.onPreferencesChanged.listener_ = callback;
    },
    removeListener: function() {
      chrome.fileManagerPrivate.onPreferencesChanged.listener_ = null;
    },
    listener_: null
  },
  getPreferences: function() {},
  setPreferences: function() {},
};

/**
 * Mock of chrome.notifications.
 * @type {Object}
 * @const
 */
chrome.notifications = {
  onButtonClicked: {
    addListener: function(callback) {
      chrome.notifications.onButtonClicked.listener_ = callback;
    },
    removeListener: function() {
      chrome.notifications.onButtonClicked.listener_ = null;
    },
    listener_: null
  },
};

/** Stub out file URLs handling. */
window.webkitResolveLocalFileSystemURL = (url, callback) => {
  callback({name: url});
};

/** As we don't have the strings just make up something instead. */
window.str = (...args) => {
  return args.join(' ');
};
window.strf = window.str;

// Set up the test components.
function setUp() {
  // Make ProgressCenterHandler.
  progressCenter = new MockProgressCenter();
  handler = new DriveSyncHandler(progressCenter);
}

// Test that in general case item IDs produced for errors are unique.
function testUniqueErrorIds() {
  // Dispatch an event.
  chrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, Object.keys(progressCenter.items).length);

  // Dispatch another event.
  chrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'service_unavailable',
    fileUrl: '',
  });

  // Check that this created second item.
  assertEquals(2, Object.keys(progressCenter.items).length);
}

// Test that item IDs produced for quota errors are same.
function testErrorDedupe() {
  // Dispatch an event.
  chrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this created one item.
  assertEquals(1, Object.keys(progressCenter.items).length);

  // Dispatch another event.
  chrome.fileManagerPrivate.onDriveSyncError.listener_({
    type: 'no_server_space',
    fileUrl: '',
  });

  // Check that this created second item.
  assertEquals(1, Object.keys(progressCenter.items).length);
}
