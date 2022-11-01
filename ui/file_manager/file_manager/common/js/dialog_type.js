// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 *
 * @enum {string}
 * @const
 */
export const DialogType = {
  SELECT_FOLDER: 'folder',
  SELECT_UPLOAD_FOLDER: 'upload-folder',
  SELECT_SAVEAS_FILE: 'saveas-file',
  SELECT_OPEN_FILE: 'open-file',
  SELECT_OPEN_MULTI_FILE: 'open-multi-file',
  FULL_PAGE: 'full-page',
};

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is modal.
 */
export function isModal(type) {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER ||
      type == DialogType.SELECT_SAVEAS_FILE ||
      type == DialogType.SELECT_OPEN_FILE ||
      type == DialogType.SELECT_OPEN_MULTI_FILE;
}

/**
 * @param {DialogType} type Dialog type.
 * @return {boolean} Whether the type is folder selection dialog.
 */
export function isFolderDialogType(type) {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER;
}
