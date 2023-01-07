// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CHOOSE_ENTRY_PROPERTY} from './choose_entry_const.js';

/**
 * Extracts parameters used by chooseEntry function.
 * @return {!chrome.fileSystem.ChooseEntryOptions}
 */
function getDialogParams() {
  const queryStr = window.location.search;
  const params = new URLSearchParams(queryStr);
  return /** @type {!chrome.fileSystem.ChooseEntryOptions} */ (
      JSON.parse(params.get('value')));
}

/**
 * Opens a file dialog. The type of the dialog is dicated by the params.
 * @param {!chrome.fileSystem.ChooseEntryOptions} params
 */
function chooseEntry() {
  const params = getDialogParams();
  return new Promise((resolve, reject) => {
    chrome.fileSystem.chooseEntry(params, (entry) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError);
      } else {
        resolve(entry);
      }
    });
  });
}

// Initializes this module by triggering chrome.fileSystem.chooseEntry call.
// This is done with the help of chooseEntry() function that returns a promise
// fulfilled once the name of the entry was selected. The entry is then set on
// the "global" variable of the background page.
chrome.runtime.getBackgroundPage(async (bgPage) => {
  // Clean up anything left over by the previous calls.
  delete bgPage[CHOOSE_ENTRY_PROPERTY];
  // Assign new entry resulting from chooseEntry() call. If the user cancels
  // assign null, to indicate cancelation.
  try {
    bgPage[CHOOSE_ENTRY_PROPERTY] = await chooseEntry();
  } catch (error) {
    if (error !== 'User cancelled') {
      console.error(error);
    }
    bgPage[CHOOSE_ENTRY_PROPERTY] = null;
  }
});
