// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxKbHandler');

goog.require('cvox.ChromeVox');
goog.require('cvox.KeyMap');
goog.require('cvox.KeySequence');
goog.require('cvox.KeyUtil');

/**
 * @fileoverview Handles user keyboard input events.
 *
 */
cvox.ChromeVoxKbHandler = {};

/**
 * The key map
 *
 * @type {cvox.KeyMap}
 */
cvox.ChromeVoxKbHandler.handlerKeyMap;

/**
 * Handler for ChromeVox commands. Returns undefined if the command does not
 * exist. Otherwise, returns the result of executing the command.
 * @type {function(string): boolean|undefined}
 */
cvox.ChromeVoxKbHandler.commandHandler;

/**
 * Loads the key bindings into the keyToFunctionsTable.
 *
 * @param {string} keyToFunctionsTable The key bindings table in JSON form.
 */
cvox.ChromeVoxKbHandler.loadKeyToFunctionsTable = function(
    keyToFunctionsTable) {
  if (!window.JSON) {
    return;
  }

  cvox.ChromeVoxKbHandler.handlerKeyMap =
      cvox.KeyMap.fromJSON(keyToFunctionsTable);
};

/**
 * Converts the key bindings table into an array that is sorted by the lengths
 * of the key bindings. After the sort, the key bindings that describe single
 * keys will come before the key bindings that describe multiple keys.
 * @param {Object<string>} keyToFunctionsTable Contains each key binding and its
 * associated function name.
 * @return {Array<Array<string>>} The sorted key bindings table in
 * array form. Each entry in the array is itself an array containing the
 * key binding and its associated function name.
 * @private
 */
cvox.ChromeVoxKbHandler.sortKeyToFunctionsTable_ = function(
    keyToFunctionsTable) {
  var sortingArray = [];

  for (var keySeqStr in keyToFunctionsTable) {
    sortingArray.push([keySeqStr, keyToFunctionsTable[keySeqStr]]);
  }

  function compareKeyStr(a, b) {
    // Compare the lengths of the key bindings.
    if (a[0].length < b[0].length) {
      return -1;
    } else if (b[0].length < a[0].length) {
      return 1;
    } else {
      // The keys are the same length. Sort lexicographically.
      return a[0].localeCompare(b[0]);
    }
  };

  sortingArray.sort(compareKeyStr);
  return sortingArray;
};


/**
 * Handles key down events.
 *
 * @param {Event} evt The key down event to process.
 * @return {boolean} True if the default action should be performed.
 */
cvox.ChromeVoxKbHandler.basicKeyDownActionsListener = function(evt) {
  var keySequence = cvox.KeyUtil.keyEventToKeySequence(evt);
  var functionName;
  if (cvox.ChromeVoxKbHandler.handlerKeyMap != undefined) {
    functionName =
        cvox.ChromeVoxKbHandler.handlerKeyMap.commandForKey(keySequence);
  } else {
    functionName = null;
  }

  // TODO (clchen): Disambiguate why functions are null. If the user pressed
  // something that is not a valid combination, make an error noise so there
  // is some feedback.

  if (!functionName) {
    return !cvox.KeyUtil.sequencing;
  }

  // If ChromeVox isn't active, ignore every command except the one
  // to toggle ChromeVox active again.
  if (!cvox.ChromeVox.isActive && functionName != 'toggleChromeVox') {
    return true;
  }

  // This is the key event handler return value - true if the event should
  // propagate and the default action should be performed, false if we eat
  // the key.
  var returnValue = true;
  var commandResult = cvox.ChromeVoxKbHandler.commandHandler(functionName);
  if (commandResult !== undefined) {
    returnValue = commandResult;
  } else if (keySequence.cvoxModifier) {
    // Modifier/prefix is active -- prevent default action
    returnValue = false;
  }

  // If the whole document is hidden from screen readers, let the app
  // catch keys as well.
  if (cvox.ChromeVox.entireDocumentIsHidden) {
    returnValue = true;
  }
  return returnValue;
};
