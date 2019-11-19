// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome;
var mockFileSystem;

/**
 * @constructor
 * @struct
 */
function MockAPIEvent() {
  /**
   * @type {!Array<!Function>}
   * @const
   */
  this.listeners_ = [];
}

/**
 * @param {!Function} callback
 */
MockAPIEvent.prototype.addListener = function(callback) {
  this.listeners_.push(callback);
};

/**
 * @param {!Function} callback
 */
MockAPIEvent.prototype.removeListener = function(callback) {
  var index = this.listeners_.indexOf(callback);
  if (index < 0) {
    throw new Error('Tried to remove an unregistered listener.');
  }
  this.listeners_.splice(index, 1);
};

/**
 * @param {...*} var_args
 */
MockAPIEvent.prototype.dispatch = function(var_args) {
  for (var i = 0; i < this.listeners_.length; i++) {
    this.listeners_[i].apply(null, arguments);
  }
};

function webkitResolveLocalFileSystemURL(url, callback) {
  var paths = Object.keys(mockFileSystem.entries);
  for (var i = 0; i < paths.length; i++) {
    var entry = mockFileSystem.entries[paths[i]];
    if (url === entry.toURL()) {
      delete chrome.runtime['lastError'];
      callback(entry);
      return;
    }
  }
  chrome.runtime.lastError = {
    name: 'Not found.'
  };
  callback(null);
}

/**
 * Use a map to track watched URLs in the test. This is not normally part of the
 * fileManagerPrivate API.
 *
 * @typedef {Object<string, boolean>}
 */
chrome.fileManagerPrivate.watchedURLs;

/**
 * Creates a mock for window.chrome. TODO(tapted): Make this an ES6 class to
 * avoid the confusing use of |this| below.
 * @constructor
 */
function MockChrome() {
  this.fileManagerPrivate = {
    onDirectoryChanged: new MockAPIEvent(),
    addFileWatch: function(entry, callback) {
      this.watchedURLs[entry.toURL()] = true;
      callback();
    },
    removeFileWatch: function(entry, callback) {
      delete this.watchedURLs[entry.toURL()];
      callback();
    },
    watchedURLs: {}
  };
  this.runtime = {};  // For lastError.
}

/**
 * Replace the real chrome APIs with a mock while suppressing closure errors.
 * This is in its own function to limit the scope of suppressions.
 *
 * @param {Object} mockChrome
 * @suppress {checkTypes|const}
 */
function replaceChromeWithMock() {
  chrome = new MockChrome();
}

function setUp() {
  replaceChromeWithMock();
  mockFileSystem = new MockFileSystem('volumeId', 'filesystem://rootURL');
  mockFileSystem.entries['/'] = MockDirectoryEntry.create(mockFileSystem, '/');
  mockFileSystem.entries['/A.txt'] =
      MockFileEntry.create(mockFileSystem, '/A.txt');
  mockFileSystem.entries['/B.txt'] =
      MockFileEntry.create(mockFileSystem, '/B.txt');
  mockFileSystem.entries['/C/'] =
      MockDirectoryEntry.create(mockFileSystem, '/C/');
  mockFileSystem.entries['/C/D.txt'] =
      MockFileEntry.create(mockFileSystem, '/C/D.txt');
}

function testAddWatcher() {
  var list = new cr.ui.ArrayDataModel([
    mockFileSystem.entries['/A.txt']
  ]);
  var watcher = new EntryListWatcher(list);
  assertArrayEquals(
      ['filesystem://rootURL/'],
      Object.keys(chrome.fileManagerPrivate.watchedURLs));
  list.push(mockFileSystem.entries['/C/D.txt']);
  assertArrayEquals(
      ['filesystem://rootURL/', 'filesystem://rootURL/C/'],
      Object.keys(chrome.fileManagerPrivate.watchedURLs).sort());
}

function testRemoveWatcher() {
  var list = new cr.ui.ArrayDataModel([
    mockFileSystem.entries['/A.txt'],
    mockFileSystem.entries['/C/D.txt']
  ]);
  var watcher = new EntryListWatcher(list);
  assertArrayEquals(
      ['filesystem://rootURL/', 'filesystem://rootURL/C/'],
      Object.keys(chrome.fileManagerPrivate.watchedURLs).sort());
  list.splice(1, 1);
  assertArrayEquals(
      ['filesystem://rootURL/'],
      Object.keys(chrome.fileManagerPrivate.watchedURLs));

}

function testEntryRemoved(callback) {
  var list = new cr.ui.ArrayDataModel([
    mockFileSystem.entries['/A.txt'],
    mockFileSystem.entries['/B.txt']
  ]);

  var watcher = new EntryListWatcher(list);
  var splicedPromise = new Promise(function(fulfill) {
    list.addEventListener('splice', fulfill);
  });

  var deletedB = mockFileSystem.entries['/B.txt'];
  delete mockFileSystem.entries['/B.txt'];
  assertArrayEquals(
      ['filesystem://rootURL/'],
      Object.keys(chrome.fileManagerPrivate.watchedURLs));
  /** @type{MockAPIEvent} */ (chrome.fileManagerPrivate.onDirectoryChanged)
      .dispatch({entry: mockFileSystem.entries['/']});

  reportPromise(splicedPromise.then(function(event) {
    assertEquals(1, event.removed.length);
    assertEquals(deletedB, event.removed[0]);
  }), callback);
}
