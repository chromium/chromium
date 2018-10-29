// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: bookmarkManagerPrivate */

/**
 * @typedef {{
 *   id: (string|undefined),
 *   parentId: (string|undefined),
 *   title: string,
 *   url: (string|undefined),
 *   children: Array
 * }}
 */
var BookmarkNodeDataElement;

/**
 * Information about the drag and drop data for use with drag and drop events.
 * @typedef {{
 *   sameProfile: boolean,
 *   elements: Array
 * }}
 */
var BookmarkNodeData;

/**
 * Collection of meta info fields.
 * @typedef {Object}
 */
var MetaInfoFields;

/**
 * @const
 */
chrome.bookmarkManagerPrivate = {};

/**
 * Copies the given bookmarks into the clipboard
 * @param {Array} idList An array of string-valued ids
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.copy = function(idList, callback) {};

/**
 * Cuts the given bookmarks into the clipboard
 * @param {Array} idList An array of string-valued ids
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.cut = function(idList, callback) {};

/**
 * Pastes bookmarks from the clipboard into the parent folder after the last
 * selected node
 * @param {string} parentId
 * @param {Array=} selectedIdList An array of string-valued ids for selected
 * bookmarks
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.paste = function(parentId, selectedIdList, callback) {};

/**
 * Whether there are any bookmarks that can be pasted
 * @param {string} parentId The ID of the folder to paste into
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.canPaste = function(parentId, callback) {};

/**
 * Sorts the children of a given folder
 * @param {string} parentId The ID of the folder to sort the children of
 */
chrome.bookmarkManagerPrivate.sortChildren = function(parentId) {};

/**
 * Gets the i18n strings for the bookmark manager
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.getStrings = function(callback) {};

/**
 * Begins dragging a set of bookmarks
 * @param {Array} idList An array of string-valued ids
 * @param {number} dragNodeIndex The index of the dragged node in |idList|
 * @param {boolean} isFromTouch True if the drag was initiated from touch
 */
chrome.bookmarkManagerPrivate.startDrag = function(idList, dragNodeIndex, isFromTouch) {};

/**
 * Performs the drop action of the drag and drop session
 * @param {string} parentId The ID of the folder that the drop was made
 * @param {number=} index The index of the position to drop at. If left out the
 * dropped items will be placed at the end of the existing children
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.drop = function(parentId, index, callback) {};

/**
 * Retrieves a bookmark hierarchy from the given node.  If the node id is
 * empty, it is the full tree.  If foldersOnly is true, it will only return
 * folders, not actual bookmarks.
 * @param {string} id ID of the root of the tree to pull.  If empty, the entire
 * tree will be returned.
 * @param {boolean} foldersOnly Pass true to only return folders.
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.getSubtree = function(id, foldersOnly, callback) {};

/**
 * Whether bookmarks can be modified
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.canEdit = function(callback) {};

/**
 * Recursively removes list of bookmarks nodes.
 * @param {Array} idList An array of string-valued ids
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.removeTrees = function(idList, callback) {};

/**
 */
chrome.bookmarkManagerPrivate.recordLaunch = function() {};

/**
 * Mimics the functionality of bookmarks.create, but will additionally set the
 * given meta info fields.
 * @param {chrome.bookmarks.CreateDetails} bookmark
 * @param {MetaInfoFields} metaInfo
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.createWithMetaInfo = function(bookmark, metaInfo, callback) {};

/**
 * Gets meta info from a bookmark node
 * @param {string=} id The id of the bookmark to retrieve meta info from. If
 * omitted meta info for all nodes is returned.
 * @param {string=} key The key for the meta info to retrieve. If omitted, all
 * fields are returned
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.getMetaInfo = function(id, key, callback) {};

/**
 * Sets a meta info value for a bookmark node
 * @param {string} id The id of the bookmark node to set the meta info on
 * @param {string} key The key of the meta info to set
 * @param {string} value The meta info to set
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.setMetaInfo = function(id, key, value, callback) {};

/**
 * Updates a set of meta info values for a bookmark node.
 * @param {string} id The id of the bookmark node to update the meta info of.
 * @param {MetaInfoFields} metaInfoChanges A set of meta info key/value pairs
 * to update.
 * @param {Function=} callback
 */
chrome.bookmarkManagerPrivate.updateMetaInfo = function(id, metaInfoChanges, callback) {};

/**
 * Performs an undo of the last change to the bookmark model
 */
chrome.bookmarkManagerPrivate.undo = function() {};

/**
 * Performs a redo of last undone change to the bookmark model
 */
chrome.bookmarkManagerPrivate.redo = function() {};

/**
 * Gets the information for the undo if available
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.getUndoInfo = function(callback) {};

/**
 * Gets the information for the redo if available
 * @param {Function} callback
 */
chrome.bookmarkManagerPrivate.getRedoInfo = function(callback) {};

/** @type {!ChromeEvent} */
chrome.bookmarkManagerPrivate.onDragEnter;

/** @type {!ChromeEvent} */
chrome.bookmarkManagerPrivate.onDragLeave;

/** @type {!ChromeEvent} */
chrome.bookmarkManagerPrivate.onDrop;

/** @type {!ChromeEvent} */
chrome.bookmarkManagerPrivate.onMetaInfoChanged;
