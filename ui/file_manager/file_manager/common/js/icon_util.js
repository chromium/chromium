// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains utils for working with icons.
 */

/**
 * @param {!chrome.fileManagerPrivate.VmType|undefined} vmType
 * @return {string}
 */
export function vmTypeToIconName(vmType) {
  if (vmType === undefined) {
    console.error('vmType: is undefined');
    return '';
  }
  switch (vmType) {
    case chrome.fileManagerPrivate.VmType.BRUSCHETTA:
      return 'bruschetta';
    case chrome.fileManagerPrivate.VmType.ARCVM:
      return 'android_files';
    case chrome.fileManagerPrivate.VmType.TERMINA:
      return 'crostini';
    default:
      console.error('Unable to determine icon for vmType: ' + vmType);
      return '';
  }
}
