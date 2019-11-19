// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to simplify working
 * with keyboard events.
 */


goog.provide('cvox.KeyUtil');
goog.provide('cvox.SimpleKeyEvent');

goog.require('Msgs');
goog.require('cvox.ChromeVox');
goog.require('cvox.KeySequence');

/**
 * @typedef {{ctrlKey: (boolean|undefined),
 *            altKey: (boolean|undefined),
 *            shiftKey: (boolean|undefined),
 *            keyCode: (number|undefined)}}
 */
cvox.SimpleKeyEvent;

/**
 * Create the namespace
 * @constructor
 */
cvox.KeyUtil = function() {
};

/**
 * The time in ms at which the ChromeVox Sticky Mode key was pressed.
 * @type {number}
 */
cvox.KeyUtil.modeKeyPressTime = 0;

/**
 * Indicates if sequencing is currently active for building a keyboard shortcut.
 * @type {boolean}
 */
cvox.KeyUtil.sequencing = false;

/**
 * The previous KeySequence when sequencing is ON.
 * @type {cvox.KeySequence}
 */
cvox.KeyUtil.prevKeySequence = null;


/**
 * The sticky key sequence.
 * @type {cvox.KeySequence}
 */
cvox.KeyUtil.stickyKeySequence = null;

/**
 * Maximum number of key codes the sequence buffer may hold. This is the max
 * length of a sequential keyboard shortcut, i.e. the number of key that can be
 * pressed one after the other while modifier keys (Cros+Shift) are held down.
 * @const
 * @type {number}
 */
cvox.KeyUtil.maxSeqLength = 2;


/**
 * Convert a key event into a Key Sequence representation.
 *
 * @param {Event|cvox.SimpleKeyEvent} keyEvent The keyEvent to convert.
 * @return {cvox.KeySequence} A key sequence representation of the key event.
 */
cvox.KeyUtil.keyEventToKeySequence = function(keyEvent) {
  var util = cvox.KeyUtil;
  if (util.prevKeySequence &&
      (util.maxSeqLength == util.prevKeySequence.length())) {
    // Reset the sequence buffer if max sequence length is reached.
    util.sequencing = false;
    util.prevKeySequence = null;
  }
  // Either we are in the middle of a key sequence (N > H), or the key prefix
  // was pressed before (Ctrl+Z), or sticky mode is enabled
  var keyIsPrefixed = util.sequencing || keyEvent['keyPrefix'] ||
      keyEvent['stickyMode'];

  // Create key sequence.
  var keySequence = new cvox.KeySequence(keyEvent);

  // Check if the Cvox key should be considered as pressed because the
  // modifier key combination is active.
  var keyWasCvox = keySequence.cvoxModifier;

  if (keyIsPrefixed || keyWasCvox) {
    if (!util.sequencing && util.isSequenceSwitchKeyCode(keySequence)) {
      // If this is the beginning of a sequence.
      util.sequencing = true;
      util.prevKeySequence = keySequence;
      return keySequence;
    } else if (util.sequencing) {
      if (util.prevKeySequence.addKeyEvent(keyEvent)) {
        keySequence = util.prevKeySequence;
        util.prevKeySequence = null;
        util.sequencing = false;
        return keySequence;
      } else {
        throw 'Think sequencing is enabled, yet util.prevKeySequence already' +
            'has two key codes' + util.prevKeySequence;
      }
    }
  } else {
    util.sequencing = false;
  }

  // Repeated keys pressed.
  var currTime = new Date().getTime();
  if (cvox.KeyUtil.isDoubleTapKey(keySequence) &&
      util.prevKeySequence &&
      keySequence.equals(util.prevKeySequence)) {
    var prevTime = util.modeKeyPressTime;
    if (prevTime > 0 && currTime - prevTime < 300) {  // Double tap
      keySequence = util.prevKeySequence;
      keySequence.doubleTap = true;
      util.prevKeySequence = null;
      util.sequencing = false;
      // Resets the search key state tracked for ChromeOS because in OOBE,
      // we never get a key up for the key down (keyCode 91).
      if (cvox.ChromeVox.isChromeOS &&
          keyEvent.keyCode == cvox.KeyUtil.getStickyKeyCode()) {
        cvox.ChromeVox.searchKeyHeld = false;
      }
      return keySequence;
    }
    // The user double tapped the sticky key but didn't do it within the
    // required time. It's possible they will try again, so keep track of the
    // time the sticky key was pressed and keep track of the corresponding
    // key sequence.
  }
  util.prevKeySequence = keySequence;
  util.modeKeyPressTime = currTime;
  return keySequence;
};

/**
 * Returns the string representation of the specified key code.
 *
 * @param {number} keyCode key code.
 * @return {string} A string representation of the key event.
 */
cvox.KeyUtil.keyCodeToString = function(keyCode) {
  if (keyCode == 17) {
    return 'Ctrl';
  }
  if (keyCode == 18) {
    return 'Alt';
  }
  if (keyCode == 16) {
    return 'Shift';
  }
  if ((keyCode == 91) || (keyCode == 93)) {
    if (cvox.ChromeVox.isChromeOS) {
      return 'Search';
    } else if (cvox.ChromeVox.isMac) {
      return 'Cmd';
    } else {
      return 'Win';
    }
  }
  // TODO(rshearer): This is a hack to work around the special casing of the
  // sticky mode string that used to happen in keyEventToString. We won't need
  // it once we move away from strings completely.
  if (keyCode == 45) {
    return 'Insert';
  }
  if (keyCode >= 65 && keyCode <= 90) {
    // A - Z
    return String.fromCharCode(keyCode);
  } else if (keyCode >= 48 && keyCode <= 57) {
    // 0 - 9
    return String.fromCharCode(keyCode);
  } else {
    // Anything else
    return '#' + keyCode;
  }
};

/**
 * Returns the keycode of a string representation of the specified modifier.
 *
 * @param {string} keyString Modifier key.
 * @return {number} Key code.
 */
cvox.KeyUtil.modStringToKeyCode = function(keyString) {
  switch (keyString) {
  case 'Ctrl':
    return 17;
  case 'Alt':
    return 18;
  case 'Shift':
    return 16;
  case 'Cmd':
  case 'Win':
    return 91;
  }
  return -1;
};

/**
 * Returns the key codes of a string respresentation of the ChromeVox modifiers.
 *
 * @return {Array<number>} Array of key codes.
 */
cvox.KeyUtil.cvoxModKeyCodes = function() {
  var modKeyCombo = cvox.ChromeVox.modKeyStr.split(/\+/g);
  var modKeyCodes = modKeyCombo.map(function(keyString) {
    return cvox.KeyUtil.modStringToKeyCode(keyString);
  });
  return modKeyCodes;
};

/**
 * Checks if the specified key code is a key used for switching into a sequence
 * mode. Sequence switch keys are specified in
 * cvox.KeyUtil.sequenceSwitchKeyCodes
 *
 * @param {!cvox.KeySequence} rhKeySeq The key sequence to check.
 * @return {boolean} true if it is a sequence switch keycode, false otherwise.
 */
cvox.KeyUtil.isSequenceSwitchKeyCode = function(rhKeySeq) {
  for (var i = 0; i < cvox.ChromeVox.sequenceSwitchKeyCodes.length; i++) {
    var lhKeySeq = cvox.ChromeVox.sequenceSwitchKeyCodes[i];
    if (lhKeySeq.equals(rhKeySeq)) {
      return true;
    }
  }
  return false;
};


/**
 * Get readable string description of the specified keycode.
 *
 * @param {number} keyCode The key code.
 * @return {string} Returns a string description.
 */
cvox.KeyUtil.getReadableNameForKeyCode = function(keyCode) {
  var msg = Msgs.getMsg.bind(Msgs);
  var cros = cvox.ChromeVox.isChromeOS;
  if (keyCode == 0) {
    return 'Power button';
  } else if (keyCode == 17) {
    return 'Control';
  } else if (keyCode == 18) {
    return 'Alt';
  } else if (keyCode == 16) {
    return 'Shift';
  } else if (keyCode == 9) {
    return 'Tab';
  } else if ((keyCode == 91) || (keyCode == 93)) {
    if (cros) {
      return 'Search';
    } else if (cvox.ChromeVox.isMac) {
      return 'Cmd';
    } else {
      return 'Win';
    }
  } else if (keyCode == 8) {
    return 'Backspace';
  } else if (keyCode == 32) {
    return 'Space';
  } else if (keyCode == 35) {
    return'end';
  } else if (keyCode == 36) {
    return 'home';
  } else if (keyCode == 37) {
    return 'Left arrow';
  } else if (keyCode == 38) {
    return 'Up arrow';
  } else if (keyCode == 39) {
    return 'Right arrow';
  } else if (keyCode == 40) {
    return 'Down arrow';
  } else if (keyCode == 45) {
    return 'Insert';
  } else if (keyCode == 13) {
    return 'Enter';
  } else if (keyCode == 27) {
    return 'Escape';
  } else if (keyCode == 112) {
    return cros ? msg('back_key') : 'F1';
  } else if (keyCode == 113) {
    return cros ? msg('forward_key') : 'F2';
  } else if (keyCode == 114) {
    return cros ? msg('refresh_key') : 'F3';
  } else if (keyCode == 115) {
    return cros ? msg('toggle_full_screen_key') : 'F4';
  } else if (keyCode == 116) {
    return cros ? msg('window_overview_key') : 'F5';
  } else if (keyCode == 117) {
    return cros ? msg('brightness_down_key') : 'F6';
  } else if (keyCode == 118) {
    return cros ? msg('brightness_up_key') : 'F7';
  } else if (keyCode == 119) {
    return cros ? msg('volume_mute_key') : 'F8';
  } else if (keyCode == 120) {
    return cros ? msg('volume_down_key') : 'F9';
  } else if (keyCode == 121) {
    return cros ? msg('volume_up_key') : 'F10';
  } else if (keyCode == 122) {
    return 'F11';
  } else if (keyCode == 123) {
    return 'F12';
  } else if (keyCode == 186) {
    return 'Semicolon';
  } else if (keyCode == 187) {
    return 'Equal sign';
  } else if (keyCode == 188) {
    return 'Comma';
  } else if (keyCode == 189) {
    return 'Dash';
  } else if (keyCode == 190) {
    return 'Period';
  } else if (keyCode == 191) {
    return 'Forward slash';
  } else if (keyCode == 192) {
    return 'Grave accent';
  } else if (keyCode == 219) {
    return 'Open bracket';
  } else if (keyCode == 220) {
    return 'Back slash';
  } else if (keyCode == 221) {
    return 'Close bracket';
  } else if (keyCode == 222) {
    return 'Single quote';
  } else if (keyCode == 115) {
    return 'Toggle full screen';
  } else if (keyCode >= 48 && keyCode <= 90) {
    return String.fromCharCode(keyCode);
  }
  return '';
};

/**
 * Get the platform specific sticky key keycode.
 *
 * @return {number} The platform specific sticky key keycode.
 */
cvox.KeyUtil.getStickyKeyCode = function() {
  // TODO (rshearer): This should not be hard-coded here.
  var stickyKeyCode = 45; // Insert for Linux and Windows
  if (cvox.ChromeVox.isChromeOS || cvox.ChromeVox.isMac) {
    stickyKeyCode = 91; // GUI key (Search/Cmd) for ChromeOs and Mac
  }
  return stickyKeyCode;
};


/**
 * Get readable string description for an internal string representation of a
 * key or a keyboard shortcut.
 *
 * @param {string} keyStr The internal string repsentation of a key or
 *     a keyboard shortcut.
 * @return {?string} Readable string representation of the input.
 */
cvox.KeyUtil.getReadableNameForStr = function(keyStr) {
  // TODO (clchen): Refactor this function away since it is no longer used.
  return null;
};


/**
 * Creates a string representation of a KeySequence.
 * A KeySequence  with a keyCode of 76 ('L') and the control and alt keys down
 * would return the string 'Ctrl+Alt+L', for example. A key code that doesn't
 * correspond to a letter or number will typically return a string with a
 * pound and then its keyCode, like '#39' for Right Arrow. However,
 * if the opt_readableKeyCode option is specified, the key code will return a
 * readable string description like 'Right Arrow' instead of '#39'.
 *
 * The modifiers always come in this order:
 *
 *   Ctrl
 *   Alt
 *   Shift
 *   Meta
 *
 * @param {cvox.KeySequence} keySequence The KeySequence object.
 * @param {boolean=} opt_readableKeyCode Whether or not to return a readable
 * string description instead of a string with a pound symbol and a keycode.
 * Default is false.
 * @param {boolean=} opt_modifiers Restrict printout to only modifiers. Defaults
 * to false.
 * @return {string} Readable string representation of the KeySequence object.
 */
cvox.KeyUtil.keySequenceToString = function(
    keySequence, opt_readableKeyCode, opt_modifiers) {
  // TODO(rshearer): Move this method and the getReadableNameForKeyCode and the
  // method to KeySequence after we refactor isModifierActive (when the modifie
  // key becomes customizable and isn't stored as a string). We can't do it
  // earlier because isModifierActive uses KeyUtil.getReadableNameForKeyCode,
  // and I don't want KeySequence to depend on KeyUtil.
  var str = '';

  var numKeys = keySequence.length();

  for (var index = 0; index < numKeys; index++) {
    if (str != '' && !opt_modifiers) {
      str += '>';
    } else if (str != '') {
      str += '+';
    }

    // This iterates through the sequence. Either we're on the first key
    // pressed or the second
    var tempStr = '';
    for (var keyPressed in keySequence.keys) {
      // This iterates through the actual key, taking into account any
      // modifiers.
      if (!keySequence.keys[keyPressed][index]) {
        continue;
      }
      var modifier = '';
      switch (keyPressed) {
        case 'ctrlKey':
        // TODO(rshearer): This is a hack to work around the special casing
        // of the Ctrl key that used to happen in keyEventToString. We won't
        // need it once we move away from strings completely.
        modifier = 'Ctrl';
        break;
      case 'searchKeyHeld':
        var searchKey = cvox.KeyUtil.getReadableNameForKeyCode(91);
        modifier = searchKey;
        break;
      case 'altKey':
        modifier = 'Alt';
        break;
      case 'altGraphKey':
        modifier = 'AltGraph';
        break;
      case 'shiftKey':
        modifier = 'Shift';
        break;
      case 'metaKey':
        var metaKey = cvox.KeyUtil.getReadableNameForKeyCode(91);
        modifier = metaKey;
        break;
      case 'keyCode':
        var keyCode = keySequence.keys[keyPressed][index];
        // We make sure the keyCode isn't for a modifier key. If it is, then
        // we've already added that into the string above.
        if (!keySequence.isModifierKey(keyCode) && !opt_modifiers) {
          if (opt_readableKeyCode) {
            tempStr += cvox.KeyUtil.getReadableNameForKeyCode(keyCode);
          } else {
            tempStr += cvox.KeyUtil.keyCodeToString(keyCode);
          }
        }
      }
      if (str.indexOf(modifier) == -1) {
          tempStr += modifier + '+';
      }
    }
    str += tempStr;

    // Strip trailing +.
    if (str[str.length - 1] == '+') {
      str = str.slice(0, -1);
    }
  }

  if (keySequence.cvoxModifier || keySequence.prefixKey) {
    if (str != '') {
      str = 'ChromeVox+' + str;
    } else {
      str = 'Cvox';
    }
  } else if (keySequence.stickyMode) {
    if (str[str.length - 1] == '>') {
      str = str.slice(0, -1);
    }
    str = str + '+' + str;
  }
  return str;
};

/**
 * Looks up if the given key sequence is triggered via double tap.
 * @param {cvox.KeySequence} key The key.
 * @return {boolean} True if key is triggered via double tap.
 */
cvox.KeyUtil.isDoubleTapKey = function(key) {
  var isSet = false;
  var originalState = key.doubleTap;
  key.doubleTap = true;
  for (var i = 0, keySeq; keySeq = cvox.KeySequence.doubleTapCache[i]; i++) {
    if (keySeq.equals(key)) {
      isSet = true;
      break;
    }
  }
  key.doubleTap = originalState;
  return isSet;
};
