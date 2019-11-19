// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class that represents a sequence of keys entered
 * by the user.
 */


goog.provide('cvox.KeySequence');

goog.require('cvox.ChromeVox');
goog.require('cvox.PlatformFilter');


/**
 * A class to represent a sequence of keys entered by a user or affiliated with
 * a ChromeVox command.
 * This class can represent the data from both types of key sequences:
 *
 * COMMAND KEYS SPECIFIED IN A KEYMAP:
 * - Two discrete keys (at most): [Down arrow], [A, A] or [O, W] etc. Can
 *   specify one or both.
 * - Modifiers (like ctrl, alt, meta, etc)
 * - Whether or not the ChromeVox modifier key is required with the command.
 *
 * USER INPUT:
 * - Two discrete keys (at most): [Down arrow], [A, A] or [O, W] etc.
 * - Modifiers (like ctlr, alt, meta, etc)
 * - Whether or not the ChromeVox modifier key was active when the keys were
 *   entered.
 * - Whether or not a prefix key was entered before the discrete keys.
 * - Whether sticky mode was active.
 * @param {Event|Object} originalEvent The original key event entered by a user.
 * The originalEvent may or may not have parameters stickyMode and keyPrefix
 * specified. We will also accept an event-shaped object.
 * @param {boolean=} opt_cvoxModifier Whether or not the ChromeVox modifier key
 * is active. If not specified, we will try to determine whether the modifier
 * was active by looking at the originalEvent.
 * from key events when the cvox modifiers are set. Defaults to false.
 * @param {boolean=} opt_doubleTap Whether this is triggered via double tap.
 * @param {boolean=} opt_skipStripping Whether to strip cvox modifiers.
 * @constructor
 */
cvox.KeySequence = function(originalEvent, opt_cvoxModifier, opt_doubleTap,
                            opt_skipStripping) {
  /** @type {boolean} */
  this.doubleTap = !!opt_doubleTap;

  /** @type {cvox.PlatformFilter} */
  this.platformFilter;
  /** @type {boolean} */
  this.skipStripping = !!opt_skipStripping;

  if (opt_cvoxModifier == undefined) {
    this.cvoxModifier = this.isCVoxModifierActive(originalEvent);
  } else {
    this.cvoxModifier = opt_cvoxModifier;
  }
  this.stickyMode = !!originalEvent['stickyMode'];
  this.prefixKey = !!originalEvent['keyPrefix'];

  if (this.stickyMode && this.prefixKey) {
    throw 'Prefix key and sticky mode cannot both be enabled: ' + originalEvent;
  }

  var event = this.resolveChromeOSSpecialKeys_(originalEvent);

  // TODO (rshearer): We should take the user out of sticky mode if they
  // try to use the CVox modifier or prefix key.

  /**
   * Stores the key codes and modifiers for the keys in the key sequence.
   * TODO(rshearer): Consider making this structure an array of minimal
   * keyEvent-like objects instead so we don't have to worry about what happens
   * when ctrlKey.length is different from altKey.length.
   *
   * NOTE: If a modifier key is pressed by itself, we will store the keyCode
   * *and* set the appropriate modKey to be true. This mirrors the way key
   * events are created on Mac and Windows. For example, if the Meta key was
   * pressed by itself, the keys object will have:
   * {metaKey: [true], keyCode:[91]}
   *
   * @type {Object}
   */
  this.keys = {
    ctrlKey: [],
    searchKeyHeld: [],
    altKey: [],
    altGraphKey: [],
    shiftKey: [],
    metaKey: [],
    keyCode: []
  };

  this.extractKey_(event);
};


// TODO(dtseng): This is incomplete; pull once we have appropriate libs.
/**
 * Maps a keypress keycode to a keydown or keyup keycode.
 * @type {Object<number, number>}
 */
cvox.KeySequence.KEY_PRESS_CODE = {
  39: 222,
  44: 188,
  45: 189,
  46: 190,
  47: 191,
  59: 186,
  91: 219,
  92: 220,
  93: 221
};

/**
 * A cache of all key sequences that have been set as double-tappable. We need
 * this cache because repeated key down computations causes ChromeVox to become
 * less responsive. This list is small so we currently use an array.
 * @type {!Array<cvox.KeySequence>}
 */
cvox.KeySequence.doubleTapCache = [];


/**
 * Adds an additional key onto the original sequence, for use when the user
 * is entering two shortcut keys. This happens when the user presses a key,
 * releases it, and then presses a second key. Those two keys together are
 * considered part of the sequence.
 * @param {Event|Object} additionalKeyEvent The additional key to be added to
 * the original event. Should be an event or an event-shaped object.
 * @return {boolean} Whether or not we were able to add a key. Returns false
 * if there are already two keys attached to this event.
 */
cvox.KeySequence.prototype.addKeyEvent = function(additionalKeyEvent) {
  if (this.keys.keyCode.length > 1) {
    return false;
  }
  this.extractKey_(additionalKeyEvent);
  return true;
};


/**
 * Check for equality. Commands are matched based on the actual key codes
 * involved and on whether or not they both require a ChromeVox modifier key.
 *
 * If sticky mode or a prefix is active on one of the commands but not on
 * the other, then we try and match based on key code first.
 * - If both commands have the same key code and neither of them have the
 * ChromeVox modifier active then we have a match.
 * - Next we try and match with the ChromeVox modifier. If both commands have
 * the same key code, and one of them has the ChromeVox modifier and the other
 * has sticky mode or an active prefix, then we also have a match.
 * @param {!cvox.KeySequence} rhs The key sequence to compare against.
 * @return {boolean} True if equal.
 */
cvox.KeySequence.prototype.equals = function(rhs) {
  // Check to make sure the same keys with the same modifiers were pressed.
  if (!this.checkKeyEquality_(rhs)) {
    return false;
  }

  if (this.doubleTap != rhs.doubleTap) {
    return false;
  }

  // So now we know the actual keys are the same.
  // If they both have the ChromeVox modifier, or they both don't have the
  // ChromeVox modifier, then they are considered equal.
  if (this.cvoxModifier === rhs.cvoxModifier) {
    return true;
  }

  // So only one of them has the ChromeVox modifier. If the one that doesn't
  // have the ChromeVox modifier has sticky mode or the prefix key then the
  // keys are still considered equal.
  var unmodified = this.cvoxModifier ? rhs : this;
  return unmodified.stickyMode || unmodified.prefixKey;
};


/**
 * Utility method that extracts the key code and any modifiers from a given
 * event and adds them to the object map.
 * @param {Event|Object} keyEvent The keyEvent or event-shaped object to extract
 * from.
 * @private
 */
cvox.KeySequence.prototype.extractKey_ = function(keyEvent) {
  for (var prop in this.keys) {
    if (prop == 'keyCode') {
      var keyCode;
      // TODO (rshearer): This is temporary until we find a library that can
      // convert between ASCII charcodes and keycodes.
      if (keyEvent.type == 'keypress' && keyEvent[prop] >= 97 &&
          keyEvent[prop] <= 122) {
        // Alphabetic keypress. Convert to the upper case ASCII code.
        keyCode = keyEvent[prop] - 32;
      } else if (keyEvent.type == 'keypress') {
        keyCode = cvox.KeySequence.KEY_PRESS_CODE[keyEvent[prop]];
      }
      this.keys[prop].push(keyCode || keyEvent[prop]);
    } else {
      if (this.isKeyModifierActive(keyEvent, prop)) {
        this.keys[prop].push(true);
      } else {
        this.keys[prop].push(false);
      }
    }
  }
  if (this.cvoxModifier) {
    this.rationalizeKeys_();
  }
};


/**
 * Rationalizes the key codes and the ChromeVox modifier for this keySequence.
 * This means we strip out the key codes and key modifiers stored for this
 * KeySequence that are also present in the ChromeVox modifier. For example, if
 * the ChromeVox modifier keys are Ctrl+Alt, and we've determined that the
 * ChromeVox modifier is active (meaning the user has pressed Ctrl+Alt), we
 * don't want this.keys.ctrlKey = true also because that implies that this
 * KeySequence involves the ChromeVox modifier and the ctrl key being held down
 * together, which doesn't make any sense.
 * @private
 */
cvox.KeySequence.prototype.rationalizeKeys_ = function() {
  if (this.skipStripping) {
    return;
  }

  // TODO (rshearer): This is a hack. When the modifier key becomes customizable
  // then we will not have to deal with strings here.
  var modifierKeyCombo = cvox.ChromeVox.modKeyStr.split(/\+/g);

  var index = this.keys.keyCode.length - 1;
  // For each modifier that is part of the CVox modifier, remove it from keys.
  if (modifierKeyCombo.indexOf('Ctrl') != -1) {
    this.keys.ctrlKey[index] = false;
  }
  if (modifierKeyCombo.indexOf('Alt') != -1) {
    this.keys.altKey[index] = false;
  }
  if (modifierKeyCombo.indexOf('Shift') != -1) {
    this.keys.shiftKey[index] = false;
  }
  var metaKeyName = this.getMetaKeyName_();
  if (modifierKeyCombo.indexOf(metaKeyName) != -1) {
    if (metaKeyName == 'Search') {
      this.keys.searchKeyHeld[index] = false;
      // TODO(dmazzoni): http://crbug.com/404763 Get rid of the code that
      // tracks the search key and just use meta everywhere.
      this.keys.metaKey[index] = false;
    } else if (metaKeyName == 'Cmd' || metaKeyName == 'Win') {
      this.keys.metaKey[index] = false;
    }
  }
};


/**
 * Get the user-facing name for the meta key (keyCode = 91), which varies
 * depending on the platform.
 * @return {string} The user-facing string name for the meta key.
 * @private
 */
cvox.KeySequence.prototype.getMetaKeyName_ = function() {
  if (cvox.ChromeVox.isChromeOS) {
    return 'Search';
  } else if (cvox.ChromeVox.isMac) {
    return 'Cmd';
  } else {
    return 'Win';
  }
};


/**
 * Utility method that checks for equality of the modifiers (like shift and alt)
 * and the equality of key codes.
 * @param {!cvox.KeySequence} rhs The key sequence to compare against.
 * @return {boolean} True if the modifiers and key codes in the key sequence are
 * the same.
 * @private
 */
cvox.KeySequence.prototype.checkKeyEquality_ = function(rhs) {
  for (var i in this.keys) {
    for (var j = this.keys[i].length; j--;) {
      if (this.keys[i][j] !== rhs.keys[i][j])
        return false;
    }
  }
  return true;
};


/**
 * Gets first key code
 * @return {number} The first key code.
 */
cvox.KeySequence.prototype.getFirstKeyCode = function() {
  return this.keys.keyCode[0];
};


/**
 * Gets the number of keys in the sequence. Should be 1 or 2.
 * @return {number} The number of keys in the sequence.
 */
cvox.KeySequence.prototype.length = function() {
  return this.keys.keyCode.length;
};



/**
 * Checks if the specified key code represents a modifier key, i.e. Ctrl, Alt,
 * Shift, Search (on ChromeOS) or Meta.
 *
 * @param {number} keyCode key code.
 * @return {boolean} true if it is a modifier keycode, false otherwise.
 */
cvox.KeySequence.prototype.isModifierKey = function(keyCode) {
  // Shift, Ctrl, Alt, Search/LWin
  return keyCode == 16 || keyCode == 17 || keyCode == 18 || keyCode == 91 ||
      keyCode == 93;
};


/**
 * Determines whether the Cvox modifier key is active during the keyEvent.
 * @param {Event|Object} keyEvent The keyEvent or event-shaped object to check.
 * @return {boolean} Whether or not the modifier key was active during the
 * keyEvent.
 */
cvox.KeySequence.prototype.isCVoxModifierActive = function(keyEvent) {
  // TODO (rshearer): Update this when the modifier key becomes customizable
  var modifierKeyCombo = cvox.ChromeVox.modKeyStr.split(/\+/g);

  // For each modifier that is held down, remove it from the combo.
  // If the combo string becomes empty, then the user has activated the combo.
  if (this.isKeyModifierActive(keyEvent, 'ctrlKey')) {
    modifierKeyCombo = modifierKeyCombo.filter(function(modifier) {
                              return modifier != 'Ctrl';
                            });
  }
  if (this.isKeyModifierActive(keyEvent, 'altKey')) {
    modifierKeyCombo = modifierKeyCombo.filter(function(modifier) {
                                                 return modifier != 'Alt';
                                               });
  }
  if (this.isKeyModifierActive(keyEvent, 'shiftKey')) {
    modifierKeyCombo = modifierKeyCombo.filter(function(modifier) {
                                                 return modifier != 'Shift';
                                               });
  }
  if (this.isKeyModifierActive(keyEvent, 'metaKey') ||
      this.isKeyModifierActive(keyEvent, 'searchKeyHeld')) {
    var metaKeyName = this.getMetaKeyName_();
    modifierKeyCombo = modifierKeyCombo.filter(function(modifier) {
                                                 return modifier != metaKeyName;
                                               });
  }
  return (modifierKeyCombo.length == 0);
};


/**
 * Determines whether a particular key modifier (for example, ctrl or alt) is
 * active during the keyEvent.
 * @param {Event|Object} keyEvent The keyEvent or Event-shaped object to check.
 * @param {string} modifier The modifier to check.
 * @return {boolean} Whether or not the modifier key was active during the
 * keyEvent.
 */
cvox.KeySequence.prototype.isKeyModifierActive = function(keyEvent, modifier) {
  // We need to check the key event modifier and the keyCode because Linux will
  // not set the keyEvent.modKey property if it is the modKey by itself.
  // This bug filed as crbug.com/74044
  switch (modifier) {
    case 'ctrlKey':
      return (keyEvent.ctrlKey || keyEvent.keyCode == 17);
      break;
    case 'altKey':
      return (keyEvent.altKey || (keyEvent.keyCode == 18));
      break;
    case 'shiftKey':
      return (keyEvent.shiftKey || (keyEvent.keyCode == 16));
      break;
    case 'metaKey':
      return (keyEvent.metaKey || (keyEvent.keyCode == 91));
      break;
    case 'searchKeyHeld':
      return ((cvox.ChromeVox.isChromeOS && keyEvent.keyCode == 91) ||
          keyEvent['searchKeyHeld']);
      break;
  }
  return false;
};

/**
 * Returns if any modifier is active in this sequence.
 * @return {boolean} The result.
 */
cvox.KeySequence.prototype.isAnyModifierActive = function() {
  for (var modifierType in this.keys) {
    for (var i = 0; i < this.length(); i++) {
      if (this.keys[modifierType][i] && modifierType != 'keyCode') {
        return true;
      }
    }
  }
  return false;
};


/**
 * Creates a KeySequence event from a generic object.
 * @param {Object} sequenceObject The object.
 * @return {cvox.KeySequence} The created KeySequence object.
 */
cvox.KeySequence.deserialize = function(sequenceObject) {
  var firstSequenceEvent = {};

  firstSequenceEvent['stickyMode'] = (sequenceObject.stickyMode == undefined) ?
      false : sequenceObject.stickyMode;
  firstSequenceEvent['prefixKey'] = (sequenceObject.prefixKey == undefined) ?
      false : sequenceObject.prefixKey;


  var secondKeyPressed = sequenceObject.keys.keyCode.length > 1;
  var secondSequenceEvent = {};

  for (var keyPressed in sequenceObject.keys) {
    firstSequenceEvent[keyPressed] = sequenceObject.keys[keyPressed][0];
    if (secondKeyPressed) {
      secondSequenceEvent[keyPressed] = sequenceObject.keys[keyPressed][1];
    }
  }
  var skipStripping = sequenceObject.skipStripping !== undefined ?
      sequenceObject.skipStripping : true;
  var keySeq = new cvox.KeySequence(firstSequenceEvent,
      sequenceObject.cvoxModifier, sequenceObject.doubleTap, skipStripping);
  if (secondKeyPressed) {
    cvox.ChromeVox.sequenceSwitchKeyCodes.push(
        new cvox.KeySequence(firstSequenceEvent, sequenceObject.cvoxModifier));
    keySeq.addKeyEvent(secondSequenceEvent);
  }

  if (sequenceObject.doubleTap) {
    cvox.KeySequence.doubleTapCache.push(keySeq);
  }

  return keySeq;
};


/**
 * Creates a KeySequence event from a given string. The string should be in the
 * standard key sequence format described in keyUtil.keySequenceToString and
 * used in the key map JSON files.
 * @param {string} keyStr The string representation of a key sequence.
 * @return {!cvox.KeySequence} The created KeySequence object.
 */
cvox.KeySequence.fromStr = function(keyStr) {
  var sequenceEvent = {};
  var secondSequenceEvent = {};

  var secondKeyPressed;
  if (keyStr.indexOf('>') == -1) {
    secondKeyPressed = false;
  } else {
    secondKeyPressed = true;
  }

  var cvoxPressed = false;
  sequenceEvent['stickyMode'] = false;
  sequenceEvent['prefixKey'] = false;

  var tokens = keyStr.split('+');
  for (var i = 0; i < tokens.length; i++) {
    var seqs = tokens[i].split('>');
    for (var j = 0; j < seqs.length; j++) {
      if (seqs[j].charAt(0) == '#') {
        var keyCode = parseInt(seqs[j].substr(1), 10);
        if (j > 0) {
          secondSequenceEvent['keyCode'] = keyCode;
        } else {
          sequenceEvent['keyCode'] = keyCode;
        }
      }
      var keyName = seqs[j];
      if (seqs[j].length == 1) {
        // Key is A/B/C...1/2/3 and we don't need to worry about setting
        // modifiers.
        if (j > 0) {
          secondSequenceEvent['keyCode'] = seqs[j].charCodeAt(0);
        } else {
          sequenceEvent['keyCode'] = seqs[j].charCodeAt(0);
        }
      } else {
        // Key is a modifier key
        if (j > 0) {
          cvox.KeySequence.setModifiersOnEvent_(keyName, secondSequenceEvent);
          if (keyName == 'Cvox') {
            cvoxPressed = true;
          }
        } else {
          cvox.KeySequence.setModifiersOnEvent_(keyName, sequenceEvent);
          if (keyName == 'Cvox') {
            cvoxPressed = true;
          }
        }
      }
    }
  }
  var keySeq = new cvox.KeySequence(sequenceEvent, cvoxPressed);
  if (secondKeyPressed) {
    keySeq.addKeyEvent(secondSequenceEvent);
  }
  return keySeq;
};


/**
 * Utility method for populating the modifiers on an event object that will be
 * used to create a KeySequence.
 * @param {string} keyName A particular modifier key name (such as 'Ctrl').
 * @param {Object} seqEvent The event to populate.
 * @private
 */
cvox.KeySequence.setModifiersOnEvent_ = function(keyName, seqEvent) {
  if (keyName == 'Ctrl') {
    seqEvent['ctrlKey'] = true;
    seqEvent['keyCode'] = 17;
  } else if (keyName == 'Alt') {
    seqEvent['altKey'] = true;
    seqEvent['keyCode'] = 18;
  } else if (keyName == 'Shift') {
    seqEvent['shiftKey'] = true;
    seqEvent['keyCode'] = 16;
  } else if (keyName == 'Search') {
    seqEvent['searchKeyHeld'] = true;
    seqEvent['keyCode'] = 91;
  } else if (keyName == 'Cmd') {
    seqEvent['metaKey'] = true;
    seqEvent['keyCode'] = 91;
  } else if (keyName == 'Win') {
    seqEvent['metaKey'] = true;
    seqEvent['keyCode'] = 91;
  } else if (keyName == 'Insert') {
    seqEvent['keyCode'] = 45;
  }
};


/**
 * Used to resolve special ChromeOS keys (see link for more detail).
 * http://crbug.com/162268
 * @param {Object} originalEvent The event.
 * @return {Object} The resolved event.
 * @private
 */
cvox.KeySequence.prototype.resolveChromeOSSpecialKeys_ =
    function(originalEvent) {
  if (!this.cvoxModifier || this.stickyMode || this.prefixKey ||
      !cvox.ChromeVox.isChromeOS) {
    return originalEvent;
  }
  var evt = {};
  for (var key in originalEvent) {
    evt[key] = originalEvent[key];
  }
  switch (evt['keyCode']) {
    case 33:  // Page up.
      evt['keyCode'] = 38;  // Up arrow.
      break;
    case 34:  // Page down.
      evt['keyCode'] = 40;  // Down arrow.
      break;
    case 35:  // End.
      evt['keyCode'] = 39;  // Right arrow.
      break;
    case 36:  // Home.
      evt['keyCode'] = 37;  // Left arrow.
      break;
    case 45:  // Insert.
      evt['keyCode'] = 190;  // Period.
      break;
    case 46:  // Delete.
      evt['keyCode'] = 8;  // Backspace.
      break;
    case 112:  // F1.
      evt['keyCode'] = 49;  // 1.
      break;
    case 113:  // F2.
      evt['keyCode'] = 50;  // 2.
      break;
    case 114:  // F3.
      evt['keyCode'] = 51;  // 3.
      break;
    case 115:  // F4.
      evt['keyCode'] = 52;  // 4.
      break;
    case 116:  // F5.
      evt['keyCode'] = 53;  // 5.
      break;
    case 117:  // F6.
      evt['keyCode'] = 54;  // 6.
      break;
    case 118:  // F7.
      evt['keyCode'] = 55;  // 7.
      break;
    case 119:  // F8.
      evt['keyCode'] = 56;  // 8.
      break;
    case 120:  // F9.
      evt['keyCode'] = 57;  // 9.
      break;
    case 121:  // F10.
      evt['keyCode'] = 48;  // 0.
      break;
    case 122:  // F11
      evt['keyCode'] = 189;  // Hyphen.
      break;
    case 123:  // F12
      evt['keyCode'] = 187;  // Equals.
      break;
  }
  return evt;
};
