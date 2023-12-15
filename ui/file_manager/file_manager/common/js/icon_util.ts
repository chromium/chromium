// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ICON_TYPES} from '../../foreground/js/constants.js';


/**
 * @fileoverview This file contains utils for working with icons.
 */

/** Return icon name for the VM type. */
export function vmTypeToIconName(vmType: chrome.fileManagerPrivate.VmType|
                                 undefined): string {
  if (vmType === undefined) {
    console.error('vmType: is undefined');
    return '';
  }
  switch (vmType) {
    case chrome.fileManagerPrivate.VmType.BRUSCHETTA:
      return ICON_TYPES.BRUSCHETTA;
    case chrome.fileManagerPrivate.VmType.ARCVM:
      return ICON_TYPES.ANDROID_FILES;
    case chrome.fileManagerPrivate.VmType.TERMINA:
      return ICON_TYPES.CROSTINI;
    default:
      console.error('Unable to determine icon for vmType: ' + vmType);
      return '';
  }
}
