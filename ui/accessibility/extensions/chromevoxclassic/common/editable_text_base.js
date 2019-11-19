// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxEditableTextBase');
goog.provide('cvox.TextChangeEvent');
goog.provide('cvox.TypingEcho');

goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.TtsInterface');
goog.require('goog.i18n.MessageFormat');


/**
 * @fileoverview Generalized logic for providing spoken feedback when editing
 * text fields, both single and multiline fields.
 *
 * {@code ChromeVoxEditableTextBase} is a generalized class that takes the
 * current state in the form of a text string, a cursor start location and a
 * cursor end location, and calls a speak method with the resulting text to
 * be spoken.  This class can be used directly for single line fields or
 * extended to override methods that extract lines for multiline fields
 * or to provide other customizations.
 */


/**
 * A class containing the information needed to speak
 * a text change event to the user.
 *
 * @constructor
 * @param {string} newValue The new string value of the editable text control.
 * @param {number} newStart The new 0-based start cursor/selection index.
 * @param {number} newEnd The new 0-based end cursor/selection index.
 * @param {boolean} triggeredByUser .
 */
cvox.TextChangeEvent = function(newValue, newStart, newEnd, triggeredByUser) {
  this.value = newValue;
  this.start = newStart;
  this.end = newEnd;
  this.triggeredByUser = triggeredByUser;

  // Adjust offsets to be in left to right order.
  if (this.start > this.end) {
    var tempOffset = this.end;
    this.end = this.start;
    this.start = tempOffset;
  }
};


/**
 * A list of typing echo options.
 * This defines the way typed characters get spoken.
 * CHARACTER: echoes typed characters.
 * WORD: echoes a word once a breaking character is typed (i.e. spacebar).
 * CHARACTER_AND_WORD: combines CHARACTER and WORD behavior.
 * NONE: speaks nothing when typing.
 * COUNT: The number of possible echo levels.
 * @enum
 */
cvox.TypingEcho = {
  CHARACTER: 0,
  WORD: 1,
  CHARACTER_AND_WORD: 2,
  NONE: 3,
  COUNT: 4
};


/**
 * @param {number} cur Current typing echo.
 * @return {number} Next typing echo.
 */
cvox.TypingEcho.cycle = function(cur) {
  return (cur + 1) % cvox.TypingEcho.COUNT;
};


/**
 * Return if characters should be spoken given the typing echo option.
 * @param {number} typingEcho Typing echo option.
 * @return {boolean} Whether the character should be spoken.
 */
cvox.TypingEcho.shouldSpeakChar = function(typingEcho) {
  return typingEcho == cvox.TypingEcho.CHARACTER_AND_WORD ||
      typingEcho == cvox.TypingEcho.CHARACTER;
};


/**
 * A class representing an abstracted editable text control.
 * @param {string} value The string value of the editable text control.
 * @param {number} start The 0-based start cursor/selection index.
 * @param {number} end The 0-based end cursor/selection index.
 * @param {boolean} isPassword Whether the text control if a password field.
 * @param {cvox.TtsInterface} tts A TTS object.
 * @constructor
 */
cvox.ChromeVoxEditableTextBase = function(value, start, end, isPassword, tts) {
  /**
   * Current value of the text field.
   * @type {string}
   * @protected
   */
  this.value = value;

  /**
   * 0-based selection start index.
   * @type {number}
   * @protected
   */
  this.start = start;

  /**
   * 0-based selection end index.
   * @type {number}
   * @protected
   */
  this.end = end;

  /**
   * True if this is a password field.
   * @type {boolean}
   * @protected
   */
  this.isPassword = isPassword;

  /**
   * Text-to-speech object implementing speak() and stop() methods.
   * @type {cvox.TtsInterface}
   * @protected
   */
  this.tts = tts;

  /**
   * Whether or not the text field is multiline.
   * @type {boolean}
   * @protected
   */
  this.multiline = false;

  /**
   * Whether or not the last update to the text and selection was described.
   *
   * Some consumers of this flag like |ChromeVoxEventWatcher| depend on and
   * react to when this flag is false by generating alternative feedback.
   * @type {boolean}
   */
  this.lastChangeDescribed = false;

};


/**
 * Performs setup for this element.
 */
cvox.ChromeVoxEditableTextBase.prototype.setup = function() {};


/**
 * Performs teardown for this element.
 */
cvox.ChromeVoxEditableTextBase.prototype.teardown = function() {};


/**
 * Whether or not moving the cursor from one character to another considers
 * the cursor to be a block (false) or an i-beam (true).
 *
 * If the cursor is a block, then the value of the character to the right
 * of the cursor index is always read when the cursor moves, no matter what
 * the previous cursor location was - this is how PC screenreaders work.
 *
 * If the cursor is an i-beam, moving the cursor by one character reads the
 * character that was crossed over, which may be the character to the left or
 * right of the new cursor index depending on the direction.
 *
 * If the current platform is a Mac, we will use an i-beam cursor. If not,
 * then we will use the block cursor.
 *
 * @type {boolean}
 */
cvox.ChromeVoxEditableTextBase.useIBeamCursor = cvox.ChromeVox.isMac;


/**
 * Switches on or off typing echo based on events. When set, editable text
 * updates for single-character insertions are handled in event watcher's key
 * press handler.
 * @type {boolean}
 */
cvox.ChromeVoxEditableTextBase.eventTypingEcho = false;


/**
 * The maximum number of characters that are short enough to speak in response
 * to an event. For example, if the user selects "Hello", we will speak
 * "Hello, selected", but if the user selects 1000 characters, we will speak
 * "text selected" instead.
 *
 * @type {number}
 */
cvox.ChromeVoxEditableTextBase.prototype.maxShortPhraseLen = 60;


/**
 * Get the line number corresponding to a particular index.
 * Default implementation that can be overridden by subclasses.
 * @param {number} index The 0-based character index.
 * @return {number} The 0-based line number corresponding to that character.
 */
cvox.ChromeVoxEditableTextBase.prototype.getLineIndex = function(index) {
  return 0;
};


/**
 * Get the start character index of a line.
 * Default implementation that can be overridden by subclasses.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the first character in this line.
 */
cvox.ChromeVoxEditableTextBase.prototype.getLineStart = function(index) {
  return 0;
};


/**
 * Get the end character index of a line.
 * Default implementation that can be overridden by subclasses.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the end of this line.
 */
cvox.ChromeVoxEditableTextBase.prototype.getLineEnd = function(index) {
  return this.value.length;
};


/**
 * Get the full text of the current line.
 * @param {number} index The 0-based line index.
 * @return {string} The text of the line.
 */
cvox.ChromeVoxEditableTextBase.prototype.getLine = function(index) {
  var lineStart = this.getLineStart(index);
  var lineEnd = this.getLineEnd(index);
  return this.value.substr(lineStart, lineEnd - lineStart);
};


/**
 * @param {string} ch The character to test.
 * @return {boolean} True if a character is whitespace.
 */
cvox.ChromeVoxEditableTextBase.prototype.isWhitespaceChar = function(ch) {
  return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
};


/**
 * @param {string} ch The character to test.
 * @return {boolean} True if a character breaks a word, used to determine
 *     if the previous word should be spoken.
 */
cvox.ChromeVoxEditableTextBase.prototype.isWordBreakChar = function(ch) {
  return !!ch.match(/^\W$/);
};


/**
 * @param {cvox.TextChangeEvent} evt The new text changed event to test.
 * @return {boolean} True if the event, when compared to the previous text,
 * should trigger description.
 */
cvox.ChromeVoxEditableTextBase.prototype.shouldDescribeChange = function(evt) {
  if (evt.value == this.value &&
      evt.start == this.start &&
      evt.end == this.end) {
    return false;
  }
  return true;
};


/**
 * Speak text, but if it's a single character, describe the character.
 * @param {string} str The string to speak.
 * @param {boolean=} opt_triggeredByUser True if the speech was triggered by a
 * user action.
 * @param {Object=} opt_personality Personality used to speak text.
 */
cvox.ChromeVoxEditableTextBase.prototype.speak =
    function(str, opt_triggeredByUser, opt_personality) {
  if (!str) {
    return;
  }
  var queueMode = cvox.QueueMode.QUEUE;
  if (opt_triggeredByUser === true) {
    queueMode = cvox.QueueMode.FLUSH;
  }
  this.tts.speak(str, queueMode, opt_personality || {});
};


/**
 * Update the state of the text and selection and describe any changes as
 * appropriate.
 *
 * @param {cvox.TextChangeEvent} evt The text change event.
 */
cvox.ChromeVoxEditableTextBase.prototype.changed = function(evt) {
  if (!this.shouldDescribeChange(evt)) {
    this.lastChangeDescribed = false;
    return;
  }

  if (evt.value == this.value) {
    this.describeSelectionChanged(evt);
  } else {
    this.describeTextChanged(evt);
  }
  this.lastChangeDescribed = true;

  this.value = evt.value;
  this.start = evt.start;
  this.end = evt.end;
};


/**
 * Describe a change in the selection or cursor position when the text
 * stays the same.
 * @param {cvox.TextChangeEvent} evt The text change event.
 */
cvox.ChromeVoxEditableTextBase.prototype.describeSelectionChanged =
    function(evt) {
  // TODO(deboer): Factor this into two function:
  //   - one to determine the selection event
  //   - one to speak

  if (this.isPassword) {
    this.speak((new goog.i18n.MessageFormat(Msgs.getMsg('dot'))
        .format({'COUNT': 1})), evt.triggeredByUser);
    return;
  }
  if (evt.start == evt.end) {
    // It's currently a cursor.
    if (this.start != this.end) {
      // It was previously a selection, so just announce 'unselected'.
      this.speak(Msgs.getMsg('Unselected'), evt.triggeredByUser);
    } else if (this.getLineIndex(this.start) !=
        this.getLineIndex(evt.start)) {
      // Moved to a different line; read it.
      var lineValue = this.getLine(this.getLineIndex(evt.start));
      if (lineValue == '') {
        lineValue = Msgs.getMsg('text_box_blank');
      } else if (/^\s+$/.test(lineValue)) {
        lineValue = Msgs.getMsg('text_box_whitespace');
      }
      this.speak(lineValue, evt.triggeredByUser);
    } else if (this.start == evt.start + 1 ||
        this.start == evt.start - 1) {
      // Moved by one character; read it.
      if (!cvox.ChromeVoxEditableTextBase.useIBeamCursor) {
        if (evt.start == this.value.length) {
          if (cvox.ChromeVox.verbosity == cvox.VERBOSITY_VERBOSE) {
            this.speak(Msgs.getMsg('end_of_text_verbose'),
                       evt.triggeredByUser);
          } else {
            this.speak(Msgs.getMsg('end_of_text_brief'),
                       evt.triggeredByUser);
          }
        } else {
          this.speak(this.value.substr(evt.start, 1),
                     evt.triggeredByUser,
                     {'phoneticCharacters': evt.triggeredByUser});
        }
      } else {
        this.speak(this.value.substr(Math.min(this.start, evt.start), 1),
            evt.triggeredByUser,
            {'phoneticCharacters': evt.triggeredByUser});
      }
    } else {
      // Moved by more than one character. Read all characters crossed.
      this.speak(this.value.substr(Math.min(this.start, evt.start),
          Math.abs(this.start - evt.start)), evt.triggeredByUser);
    }
  } else {
    // It's currently a selection.
    if (this.start + 1 == evt.start &&
        this.end == this.value.length &&
        evt.end == this.value.length) {
      // Autocomplete: the user typed one character of autocompleted text.
      this.speak(this.value.substr(this.start, 1), evt.triggeredByUser);
      this.speak(this.value.substr(evt.start));
    } else if (this.start == this.end) {
      // It was previously a cursor.
      this.speak(this.value.substr(evt.start, evt.end - evt.start),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('selected'));
    } else if (this.start == evt.start && this.end < evt.end) {
      this.speak(this.value.substr(this.end, evt.end - this.end),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('added_to_selection'));
    } else if (this.start == evt.start && this.end > evt.end) {
      this.speak(this.value.substr(evt.end, this.end - evt.end),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('removed_from_selection'));
    } else if (this.end == evt.end && this.start > evt.start) {
      this.speak(this.value.substr(evt.start, this.start - evt.start),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('added_to_selection'));
    } else if (this.end == evt.end && this.start < evt.start) {
      this.speak(this.value.substr(this.start, evt.start - this.start),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('removed_from_selection'));
    } else {
      // The selection changed but it wasn't an obvious extension of
      // a previous selection. Just read the new selection.
      this.speak(this.value.substr(evt.start, evt.end - evt.start),
                 evt.triggeredByUser);
      this.speak(Msgs.getMsg('selected'));
    }
  }
};


/**
 * Describe a change where the text changes.
 * @param {cvox.TextChangeEvent} evt The text change event.
 */
cvox.ChromeVoxEditableTextBase.prototype.describeTextChanged = function(evt) {
  var personality = {};
  if (evt.value.length < this.value.length) {
    personality = cvox.AbstractTts.PERSONALITY_DELETED;
  }
  if (this.isPassword) {
    this.speak((new goog.i18n.MessageFormat(Msgs.getMsg('dot'))
        .format({'COUNT': 1})), evt.triggeredByUser, personality);
    return;
  }

  var value = this.value;
  var len = value.length;
  var newLen = evt.value.length;
  var autocompleteSuffix = '';
  // Make a copy of evtValue and evtEnd to avoid changing anything in
  // the event itself.
  var evtValue = evt.value;
  var evtEnd = evt.end;

  // First, see if there's a selection at the end that might have been
  // added by autocomplete. If so, strip it off into a separate variable.
  if (evt.start < evtEnd && evtEnd == newLen) {
    autocompleteSuffix = evtValue.substr(evt.start);
    evtValue = evtValue.substr(0, evt.start);
    evtEnd = evt.start;
  }

  // Now see if the previous selection (if any) was deleted
  // and any new text was inserted at that character position.
  // This would handle pasting and entering text by typing, both from
  // a cursor and from a selection.
  var prefixLen = this.start;
  var suffixLen = len - this.end;
  if (newLen >= prefixLen + suffixLen + (evtEnd - evt.start) &&
      evtValue.substr(0, prefixLen) == value.substr(0, prefixLen) &&
      evtValue.substr(newLen - suffixLen) == value.substr(this.end)) {
    // However, in a dynamic content editable, defer to authoritative events
    // (clipboard, key press) to reduce guess work when observing insertions.
    // Only use this logic when observing deletions (and insertion of word
    // breakers).
    // TODO(dtseng): Think about a more reliable way to do this.
    if (!(cvox.ChromeVoxEditableContentEditable &&
            this instanceof cvox.ChromeVoxEditableContentEditable) ||
        newLen < len ||
        this.isWordBreakChar(evt.value[newLen - 1] || '')) {
      this.describeTextChangedHelper(
          evt, prefixLen, suffixLen, autocompleteSuffix, personality);
    }
    return;
  }

  // Next, see if one or more characters were deleted from the previous
  // cursor position and the new cursor is in the expected place. This
  // handles backspace, forward-delete, and similar shortcuts that delete
  // a word or line.
  prefixLen = evt.start;
  suffixLen = newLen - evtEnd;
  if (this.start == this.end &&
      evt.start == evtEnd &&
      evtValue.substr(0, prefixLen) == value.substr(0, prefixLen) &&
      evtValue.substr(newLen - suffixLen) ==
      value.substr(len - suffixLen)) {
    this.describeTextChangedHelper(
        evt, prefixLen, suffixLen, autocompleteSuffix, personality);
    return;
  }

  // If all else fails, we assume the change was not the result of a normal
  // user editing operation, so we'll have to speak feedback based only
  // on the changes to the text, not the cursor position / selection.
  // First, restore the autocomplete text if any.
  evtValue += autocompleteSuffix;

  // Try to do a diff between the new and the old text. If it is a one character
  // insertion/deletion at the start or at the end, just speak that character.
  if ((evtValue.length == (value.length + 1)) ||
      ((evtValue.length + 1) == value.length)) {
    // The user added text either to the beginning or the end.
    if (evtValue.length > value.length) {
      if (evtValue.indexOf(value) == 0) {
        this.speak(evtValue[evtValue.length - 1], evt.triggeredByUser,
                   personality);
        return;
      } else if (evtValue.indexOf(value) == 1) {
        this.speak(evtValue[0], evt.triggeredByUser, personality);
        return;
      }
    }
    // The user deleted text either from the beginning or the end.
    if (evtValue.length < value.length) {
      if (value.indexOf(evtValue) == 0) {
        this.speak(value[value.length - 1], evt.triggeredByUser, personality);
        return;
      } else if (value.indexOf(evtValue) == 1) {
        this.speak(value[0], evt.triggeredByUser, personality);
        return;
      }
    }
  }

  if (this.multiline) {
    // Fall back to announce deleted but omit the text that was deleted.
    if (evt.value.length < this.value.length) {
      this.speak(Msgs.getMsg('text_deleted'),
                 evt.triggeredByUser, personality);
    }
    // The below is a somewhat loose way to deal with non-standard
    // insertions/deletions. Intentionally skip for multiline since deletion
    // announcements are covered above and insertions are non-standard (possibly
    // due to auto complete). Since content editable's often refresh content by
    // removing and inserting entire chunks of text, this type of logic often
    // results in unintended consequences such as reading all text when only one
    // character has been entered.
    return;
  }

  // If the text is short, just speak the whole thing.
  if (newLen <= this.maxShortPhraseLen) {
    this.describeTextChangedHelper(evt, 0, 0, '', personality);
    return;
  }

  // Otherwise, look for the common prefix and suffix, but back up so
  // that we can speak complete words, to be minimally confusing.
  prefixLen = 0;
  while (prefixLen < len &&
         prefixLen < newLen &&
         value[prefixLen] == evtValue[prefixLen]) {
    prefixLen++;
  }
  while (prefixLen > 0 && !this.isWordBreakChar(value[prefixLen - 1])) {
    prefixLen--;
  }

  suffixLen = 0;
  while (suffixLen < (len - prefixLen) &&
         suffixLen < (newLen - prefixLen) &&
         value[len - suffixLen - 1] == evtValue[newLen - suffixLen - 1]) {
    suffixLen++;
  }
  while (suffixLen > 0 && !this.isWordBreakChar(value[len - suffixLen])) {
    suffixLen--;
  }

  this.describeTextChangedHelper(evt, prefixLen, suffixLen, '', personality);
};


/**
 * The function called by describeTextChanged after it's figured out
 * what text was deleted, what text was inserted, and what additional
 * autocomplete text was added.
 * @param {cvox.TextChangeEvent} evt The text change event.
 * @param {number} prefixLen The number of characters in the common prefix
 *     of this.value and newValue.
 * @param {number} suffixLen The number of characters in the common suffix
 *     of this.value and newValue.
 * @param {string} autocompleteSuffix The autocomplete string that was added
 *     to the end, if any. It should be spoken at the end of the utterance
 *     describing the change.
 * @param {Object=} opt_personality Personality to speak the text.
 */
cvox.ChromeVoxEditableTextBase.prototype.describeTextChangedHelper = function(
    evt, prefixLen, suffixLen, autocompleteSuffix, opt_personality) {
  var len = this.value.length;
  var newLen = evt.value.length;
  var deletedLen = len - prefixLen - suffixLen;
  var deleted = this.value.substr(prefixLen, deletedLen);
  var insertedLen = newLen - prefixLen - suffixLen;
  var inserted = evt.value.substr(prefixLen, insertedLen);
  var utterance = '';
  var triggeredByUser = evt.triggeredByUser;

  if (insertedLen > 1) {
    utterance = inserted;
  } else if (insertedLen == 1) {
    if ((cvox.ChromeVox.typingEcho == cvox.TypingEcho.WORD ||
            cvox.ChromeVox.typingEcho == cvox.TypingEcho.CHARACTER_AND_WORD) &&
        this.isWordBreakChar(inserted) &&
        prefixLen > 0 &&
        !this.isWordBreakChar(evt.value.substr(prefixLen - 1, 1))) {
      // Speak previous word.
      var index = prefixLen;
      while (index > 0 && !this.isWordBreakChar(evt.value[index - 1])) {
        index--;
      }
      if (index < prefixLen) {
        utterance = evt.value.substr(index, prefixLen + 1 - index);
      } else {
        utterance = inserted;
        triggeredByUser = false; // Implies QUEUE_MODE_QUEUE.
      }
    } else if (cvox.ChromeVox.typingEcho == cvox.TypingEcho.CHARACTER ||
        cvox.ChromeVox.typingEcho == cvox.TypingEcho.CHARACTER_AND_WORD) {
      // This particular case is handled in event watcher. See the key press
      // handler for more details.
      utterance = cvox.ChromeVoxEditableTextBase.eventTypingEcho ? '' :
          inserted;
    }
  } else if (deletedLen > 1 && !autocompleteSuffix) {
    utterance = deleted + ', deleted';
  } else if (deletedLen == 1) {
    utterance = deleted;
  }

  if (autocompleteSuffix && utterance) {
    utterance += ', ' + autocompleteSuffix;
  } else if (autocompleteSuffix) {
    utterance = autocompleteSuffix;
  }

  if (utterance) {
    this.speak(utterance, triggeredByUser, opt_personality);
  }
};


/**
 * Moves the cursor forward by one character.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToNextCharacter =
    function() { return false; };


/**
 * Moves the cursor backward by one character.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToPreviousCharacter =
    function() { return false; };


/**
 * Moves the cursor forward by one word.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToNextWord =
    function() { return false; };


/**
 * Moves the cursor backward by one word.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToPreviousWord =
    function() { return false; };


/**
 * Moves the cursor forward by one line.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToNextLine =
    function() { return false; };


/**
 * Moves the cursor backward by one line.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToPreviousLine =
    function() { return false; };


/**
 * Moves the cursor forward by one paragraph.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToNextParagraph =
    function() { return false; };


/**
 * Moves the cursor backward by one paragraph.
 * @return {boolean} True if the action was handled.
 */
cvox.ChromeVoxEditableTextBase.prototype.moveCursorToPreviousParagraph =
    function() { return false; };


/******************************************/
