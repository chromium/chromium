// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Braille command definitions.
 * These types are adapted from Chrome's private braille API.
 * They can be found in the Chrome source repo at:
 * src/chrome/common/extensions/api/braille_display_private.idl
 * We define them here since they don't actually exist as bindings under
 * chrome.brailleDisplayPrivate.*.
 */

goog.provide('cvox.BrailleDisplayState');
goog.provide('cvox.BrailleKeyCommand');
goog.provide('cvox.BrailleKeyEvent');


/**
 * The set of commands sent from a braille display.
 * @enum {string}
 */
cvox.BrailleKeyCommand = {
  PAN_LEFT: 'pan_left',
  PAN_RIGHT: 'pan_right',
  LINE_UP: 'line_up',
  LINE_DOWN: 'line_down',
  TOP: 'top',
  BOTTOM: 'bottom',
  ROUTING: 'routing',
  SECONDARY_ROUTING: 'secondary_routing',
  DOTS: 'dots',
  STANDARD_KEY: 'standard_key'
};


/**
 * Represents a key event from a braille display.
 *
 * @typedef {{command: cvox.BrailleKeyCommand,
 *            displayPosition: (undefined|number),
 *            brailleDots: (undefined|number),
 *            standardKeyCode: (undefined|string),
 *            standardKeyChar: (undefined|string),
 *            altKey: (undefined|boolean),
 *            ctrlKey: (undefined|boolean),
 *            shiftKey: (undefined|boolean)
 *          }}
 *  command The name of the command.
 *  displayPosition The 0-based position relative to the start of the currently
 *                  displayed text.  Used for commands that involve routing
 *                  keys or similar.  The position is given in characters,
 *                  not braille cells.
 *  brailleDots Dots that were pressed for braille input commands.  Bit mask
 *              where bit 0 represents dot 1 etc.
 * standardKeyCode DOM level 4 key code.
 * standardKeyChar DOM key event character.
 * altKey Whether the alt key was pressed.
 * ctrlKey Whether the control key was pressed.
 * shiftKey Whether the shift key was pressed.
 */
cvox.BrailleKeyEvent = {};


/**
 * Returns the numeric key code for a DOM level 4 key code string.
 * NOTE: Only the key codes produced by the brailleDisplayPrivate API are
 * supported.
 * @param {string} code DOM level 4 key code.
 * @return {number|undefined} The numeric key code, or {@code undefined}
 *     if unknown.
 */
cvox.BrailleKeyEvent.keyCodeToLegacyCode = function(code) {
  return cvox.BrailleKeyEvent.legacyKeyCodeMap_[code];
};


/**
 * Returns a char value appropriate for a synthezised key event for a given
 * key code.
 * @param {string} keyCode The DOM level 4 key code.
 * @return {number} Integral character code.
 */
cvox.BrailleKeyEvent.keyCodeToCharValue = function(keyCode) {
  /** @const */
  var SPECIAL_CODES = {
    'Backspace': 0x08,
    'Tab': 0x09,
    'Enter': 0x0A
  };
  // Note, the Chrome virtual keyboard falls back on the first character of the
  // key code if the key is not one of the above.  Do the same here.
  return SPECIAL_CODES[keyCode] || keyCode.charCodeAt(0);
};


/**
 * Map from DOM level 4 key codes to legacy numeric key codes.
 * @private {Object<number>}
 */
cvox.BrailleKeyEvent.legacyKeyCodeMap_ = {
  'Backspace': 8,
  'Tab': 9,
  'Enter': 13,
  'Escape': 27,
  'Home': 36,
  'ArrowLeft': 37,
  'ArrowUp': 38,
  'ArrowRight': 39,
  'ArrowDown': 40,
  'PageUp': 33,
  'PageDown': 34,
  'End': 35,
  'Insert': 45,
  'Delete': 46
};

// Add the F1 to F12 keys.
(function() {
  for (var i = 0; i < 12; ++i) {
    cvox.BrailleKeyEvent.legacyKeyCodeMap_['F' + (i + 1)] = 112 + i;
  }
})();


/**
 * The state of a braille display as represented in the
 * chrome.brailleDisplayPrivate API.
 * @typedef {{available: boolean, textCellCount: (number|undefined)}}
 */
cvox.BrailleDisplayState;
