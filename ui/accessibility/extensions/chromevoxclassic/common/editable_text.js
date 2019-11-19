// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.ChromeVoxEditableContentEditable');
goog.provide('cvox.ChromeVoxEditableElement');
goog.provide('cvox.ChromeVoxEditableHTMLInput');
goog.provide('cvox.ChromeVoxEditableTextArea');
goog.provide('cvox.TextHandlerInterface');


goog.require('cvox.BrailleTextHandler');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ContentEditableExtractor');
goog.require('cvox.DomUtil');
goog.require('cvox.EditableTextAreaShadow');
goog.require('cvox.TextChangeEvent');
goog.require('cvox.TtsInterface');

/**
 * @fileoverview Gives the user spoken and braille feedback as they type,
 * select text, and move the cursor in editable HTML text controls, including
 * multiline controls and contenteditable regions.
 *
 * The two subclasses, ChromeVoxEditableHTMLInput and
 * ChromeVoxEditableTextArea, take a HTML input (type=text) or HTML
 * textarea node (respectively) in the constructor, and automatically
 * handle retrieving the current state of the control, including
 * computing line break information for a textarea using an offscreen
 * shadow object. It is the responsibility of the user of these classes to
 * trap key and focus events and call the update method as needed.
 *
 */


/**
 * An interface for being notified when the text changes.
 * @interface
 */
cvox.TextHandlerInterface = function() {};


/**
 * Called when text changes.
 * @param {cvox.TextChangeEvent} evt The text change event.
 */
cvox.TextHandlerInterface.prototype.changed = function(evt) {};


/**
 * A subclass of ChromeVoxEditableTextBase a text element that's part of
 * the webpage DOM. Contains common code shared by both EditableHTMLInput
 * and EditableTextArea, but that might not apply to a non-DOM text box.
 * @param {Element} node A DOM node which allows text input.
 * @param {string} value The string value of the editable text control.
 * @param {number} start The 0-based start cursor/selection index.
 * @param {number} end The 0-based end cursor/selection index.
 * @param {boolean} isPassword Whether the text control if a password field.
 * @param {cvox.TtsInterface} tts A TTS object.
 * @extends {cvox.ChromeVoxEditableTextBase}
 * @constructor
 */
cvox.ChromeVoxEditableElement = function(node, value, start, end, isPassword,
    tts) {
  goog.base(this, value, start, end, isPassword, tts);

  /**
   * An optional handler for braille output.
   * @type {cvox.BrailleTextHandler|undefined}
   * @private
   */
  this.brailleHandler_ = cvox.ChromeVox.braille ?
      new cvox.BrailleTextHandler(cvox.ChromeVox.braille) : undefined;

  /**
   * The DOM node which allows text input.
   * @type {Element}
   * @protected
   */
  this.node = node;

  /**
   * True if the description was just spoken.
   * @type {boolean}
   * @private
   */
  this.justSpokeDescription_ = false;
};
goog.inherits(cvox.ChromeVoxEditableElement,
    cvox.ChromeVoxEditableTextBase);


/** @override */
cvox.ChromeVoxEditableElement.prototype.changed = function(evt) {
  // Ignore changes to the cursor and selection if they happen immediately
  // after the description was just spoken. This avoid double-speaking when,
  // for example, a text field is focused and then a moment later the
  // contents are selected. If the value changes, though, this change will
  // not be ignored.
  if (this.justSpokeDescription_ && this.value == evt.value) {
    this.value = evt.value;
    this.start = evt.start;
    this.end = evt.end;
    this.justSpokeDescription_ = false;
  }
  goog.base(this, 'changed', evt);
  if (this.lastChangeDescribed) {
    this.brailleCurrentLine_();
  }
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.speak = function(
    str, opt_triggeredByUser, opt_personality) {
  // If there is a node associated with the editable text object,
  // make sure that node has focus before speaking it.
  if (this.node && (document.activeElement != this.node)) {
    return;
  }
  goog.base(this, 'speak', str, opt_triggeredByUser, opt_personality);
};

/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToNextCharacter = function() {
  var node = this.node;
  node.selectionEnd++;
  node.selectionStart = node.selectionEnd;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToPreviousCharacter =
    function() {
  var node = this.node;
  node.selectionStart--;
  node.selectionEnd = node.selectionStart;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToNextWord = function() {
  var node = this.node;
  var length = node.value.length;
  var re = /\W+/gm;
  var substring = node.value.substring(node.selectionEnd);
  var match = re.exec(substring);
  if (match !== null && match.index == 0) {
    // Ignore word-breaking sequences right next to the cursor.
    match = re.exec(substring);
  }
  var index = (match === null) ? length : match.index + node.selectionEnd;
  node.selectionStart = node.selectionEnd = index;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToPreviousWord = function() {
  var node = this.node;
  var length = node.value.length;
  var re = /\W+/gm;
  var substring = node.value.substring(0, node.selectionStart);
  var index = 0;
  while (re.exec(substring) !== null) {
    if (re.lastIndex < node.selectionStart) {
      index = re.lastIndex;
    }
  }
  node.selectionStart = node.selectionEnd = index;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToNextParagraph =
    function() {
  var node = this.node;
  var length = node.value.length;
  var index = node.selectionEnd >= length ? length :
      node.value.indexOf('\n', node.selectionEnd);
  if (index < 0) {
    index = length;
  }
  node.selectionStart = node.selectionEnd = index + 1;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableElement.prototype.moveCursorToPreviousParagraph =
    function() {
  var node = this.node;
  var index = node.selectionStart <= 0 ? 0 :
      node.value.lastIndexOf('\n', node.selectionStart - 2) + 1;
  if (index < 0) {
    index = 0;
  }
  node.selectionStart = node.selectionEnd = index;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};

/**
 * Shows the current line on the braille display.
 * @private
 */
cvox.ChromeVoxEditableElement.prototype.brailleCurrentLine_ = function() {
  if (this.brailleHandler_) {
    var lineIndex = this.getLineIndex(this.start);
    var line = this.getLine(lineIndex);
    // Collapsable whitespace inside the contenteditable is represented
    // as non-breaking spaces.  This confuses braille input (which relies on
    // the text being added to be the same as the text in the input field).
    // Since the non-breaking spaces are just an artifact of how
    // contenteditable is implemented, normalize to normal spaces instead.
    if (this instanceof cvox.ChromeVoxEditableContentEditable) {
      line = line.replace(/\u00A0/g, ' ');
    }
    var lineStart = this.getLineStart(lineIndex);
    var start = this.start - lineStart;
    var end = Math.min(this.end - lineStart, line.length);
    this.brailleHandler_.changed(line, start, end, this.multiline, this.node,
                                 lineStart);
  }
};

/******************************************/


/**
 * A subclass of ChromeVoxEditableElement for an HTMLInputElement.
 * @param {HTMLInputElement} node The HTMLInputElement node.
 * @param {cvox.TtsInterface} tts A TTS object.
 * @extends {cvox.ChromeVoxEditableElement}
 * @implements {cvox.TextHandlerInterface}
 * @constructor
 */
cvox.ChromeVoxEditableHTMLInput = function(node, tts) {
  this.node = node;
  this.setup();
  goog.base(this,
            node,
            node.value,
            node.selectionStart,
            node.selectionEnd,
            node.type === 'password',
            tts);
};
goog.inherits(cvox.ChromeVoxEditableHTMLInput,
    cvox.ChromeVoxEditableElement);


/**
 * Performs setup for this input node.
 * This accounts for exception-throwing behavior introduced by crbug.com/324360.
 * @override
 */
cvox.ChromeVoxEditableHTMLInput.prototype.setup = function() {
  if (!this.node) {
    return;
  }
  if (!cvox.DomUtil.doesInputSupportSelection(this.node)) {
    this.originalType = this.node.type;
    this.node.type = 'text';
  }
};


/**
 * Performs teardown for this input node.
 * This accounts for exception-throwing behavior introduced by crbug.com/324360.
 * @override
 */
cvox.ChromeVoxEditableHTMLInput.prototype.teardown = function() {
  if (this.node && this.originalType) {
    this.node.type = this.originalType;
  }
};


/**
 * Update the state of the text and selection and describe any changes as
 * appropriate.
 *
 * @param {boolean} triggeredByUser True if this was triggered by a user action.
 */
cvox.ChromeVoxEditableHTMLInput.prototype.update = function(triggeredByUser) {
  var newValue = this.node.value;
  var textChangeEvent = new cvox.TextChangeEvent(newValue,
                                                 this.node.selectionStart,
                                                 this.node.selectionEnd,
                                                 triggeredByUser);
  this.changed(textChangeEvent);
};


/******************************************/


/**
 * A subclass of ChromeVoxEditableElement for an HTMLTextAreaElement.
 * @param {HTMLTextAreaElement} node The HTMLTextAreaElement node.
 * @param {cvox.TtsInterface} tts A TTS object.
 * @extends {cvox.ChromeVoxEditableElement}
 * @implements {cvox.TextHandlerInterface}
 * @constructor
 */
cvox.ChromeVoxEditableTextArea = function(node, tts) {
  goog.base(this, node, node.value, node.selectionStart, node.selectionEnd,
      false /* isPassword */, tts);
  this.multiline = true;

  /**
   * True if the shadow is up to date with the current value of this text area.
   * @type {boolean}
   * @private
   */
  this.shadowIsCurrent_ = false;
};
goog.inherits(cvox.ChromeVoxEditableTextArea,
    cvox.ChromeVoxEditableElement);


/**
 * An offscreen div used to compute the line numbers. A single div is
 * shared by all instances of the class.
 * @type {!cvox.EditableTextAreaShadow|undefined}
 * @private
 */
cvox.ChromeVoxEditableTextArea.shadow_;


/**
 * Update the state of the text and selection and describe any changes as
 * appropriate.
 *
 * @param {boolean} triggeredByUser True if this was triggered by a user action.
 */
cvox.ChromeVoxEditableTextArea.prototype.update = function(triggeredByUser) {
  if (this.node.value != this.value) {
    this.shadowIsCurrent_ = false;
  }
  var textChangeEvent = new cvox.TextChangeEvent(this.node.value,
      this.node.selectionStart, this.node.selectionEnd, triggeredByUser);
  this.changed(textChangeEvent);
};


/**
 * Get the line number corresponding to a particular index.
 * @param {number} index The 0-based character index.
 * @return {number} The 0-based line number corresponding to that character.
 */
cvox.ChromeVoxEditableTextArea.prototype.getLineIndex = function(index) {
  return this.getShadow().getLineIndex(index);
};


/**
 * Get the start character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the first character in this line.
 */
cvox.ChromeVoxEditableTextArea.prototype.getLineStart = function(index) {
  return this.getShadow().getLineStart(index);
};


/**
 * Get the end character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the end of this line.
 */
cvox.ChromeVoxEditableTextArea.prototype.getLineEnd = function(index) {
  return this.getShadow().getLineEnd(index);
};


/**
 * Update the shadow object, an offscreen div used to compute line numbers.
 * @return {!cvox.EditableTextAreaShadow} The shadow object.
 */
cvox.ChromeVoxEditableTextArea.prototype.getShadow = function() {
  var shadow = cvox.ChromeVoxEditableTextArea.shadow_;
  if (!shadow) {
    shadow = cvox.ChromeVoxEditableTextArea.shadow_ =
        new cvox.EditableTextAreaShadow();
  }
  if (!this.shadowIsCurrent_) {
    shadow.update(this.node);
    this.shadowIsCurrent_ = true;
  }
  return shadow;
};


/** @override */
cvox.ChromeVoxEditableTextArea.prototype.moveCursorToNextLine = function() {
  var node = this.node;
  var length = node.value.length;
  if (node.selectionEnd >= length) {
    return false;
  }
  var shadow = this.getShadow();
  var lineIndex = shadow.getLineIndex(node.selectionEnd);
  var lineStart = shadow.getLineStart(lineIndex);
  var offset = node.selectionEnd - lineStart;
  var lastLine = (length == 0) ? 0 : shadow.getLineIndex(length - 1);
  var newCursorPosition = (lineIndex >= lastLine) ? length :
      Math.min(shadow.getLineStart(lineIndex + 1) + offset,
          shadow.getLineEnd(lineIndex + 1));
  node.selectionStart = node.selectionEnd = newCursorPosition;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableTextArea.prototype.moveCursorToPreviousLine = function() {
  var node = this.node;
  if (node.selectionStart <= 0) {
    return false;
  }
  var shadow = this.getShadow();
  var lineIndex = shadow.getLineIndex(node.selectionStart);
  var lineStart = shadow.getLineStart(lineIndex);
  var offset = node.selectionStart - lineStart;
  var newCursorPosition = (lineIndex <= 0) ? 0 :
      Math.min(shadow.getLineStart(lineIndex - 1) + offset,
          shadow.getLineEnd(lineIndex - 1));
  node.selectionStart = node.selectionEnd = newCursorPosition;
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/******************************************/


/**
 * A subclass of ChromeVoxEditableElement for elements that are contentEditable.
 * This is also used for a region of HTML with the ARIA role of "textbox",
 * so that an author can create a pure-JavaScript editable text object - this
 * will work the same as contentEditable as long as the DOM selection is
 * updated properly within the textbox when it has focus.
 * @param {Element} node The root contentEditable node.
 * @param {cvox.TtsInterface} tts A TTS object.
 * @extends {cvox.ChromeVoxEditableElement}
 * @implements {cvox.TextHandlerInterface}
 * @constructor
 */
cvox.ChromeVoxEditableContentEditable = function(node, tts) {
  goog.base(this, node, '', 0, 0, false /* isPassword */, tts);


  /**
   * True if the ContentEditableExtractor is current with this field's data.
   * @type {boolean}
   * @private
   */
  this.extractorIsCurrent_ = false;

  var extractor = this.getExtractor();
  this.value = extractor.getText();
  this.start = extractor.getStartIndex();
  this.end = extractor.getEndIndex();
  this.multiline = true;
};
goog.inherits(cvox.ChromeVoxEditableContentEditable,
    cvox.ChromeVoxEditableElement);

/**
 * A helper used to compute the line numbers. A single object is
 * shared by all instances of the class.
 * @type {!cvox.ContentEditableExtractor|undefined}
 * @private
 */
cvox.ChromeVoxEditableContentEditable.extractor_;


/**
 * Update the state of the text and selection and describe any changes as
 * appropriate.
 *
 * @param {boolean} triggeredByUser True if this was triggered by a user action.
 */
cvox.ChromeVoxEditableContentEditable.prototype.update =
    function(triggeredByUser) {
  this.extractorIsCurrent_ = false;
  var textChangeEvent = new cvox.TextChangeEvent(
      this.getExtractor().getText(),
      this.getExtractor().getStartIndex(),
      this.getExtractor().getEndIndex(),
      triggeredByUser);
  this.changed(textChangeEvent);
};


/**
 * Get the line number corresponding to a particular index.
 * @param {number} index The 0-based character index.
 * @return {number} The 0-based line number corresponding to that character.
 */
cvox.ChromeVoxEditableContentEditable.prototype.getLineIndex = function(index) {
  return this.getExtractor().getLineIndex(index);
};


/**
 * Get the start character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the first character in this line.
 */
cvox.ChromeVoxEditableContentEditable.prototype.getLineStart = function(index) {
  return this.getExtractor().getLineStart(index);
};


/**
 * Get the end character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the end of this line.
 */
cvox.ChromeVoxEditableContentEditable.prototype.getLineEnd = function(index) {
  return this.getExtractor().getLineEnd(index);
};


/**
 * Update the extractor object, an offscreen div used to compute line numbers.
 * @return {!cvox.ContentEditableExtractor} The extractor object.
 */
cvox.ChromeVoxEditableContentEditable.prototype.getExtractor = function() {
  var extractor = cvox.ChromeVoxEditableContentEditable.extractor_;
  if (!extractor) {
    extractor = cvox.ChromeVoxEditableContentEditable.extractor_ =
        new cvox.ContentEditableExtractor();
  }
  if (!this.extractorIsCurrent_) {
    extractor.update(this.node);
    this.extractorIsCurrent_ = true;
  }
  return extractor;
};


/** @override */
cvox.ChromeVoxEditableContentEditable.prototype.changed =
    function(evt) {
  if (!evt.triggeredByUser) {
    return;
  }
  // Take over here if we can't describe a change; assume it's a blank line.
  if (!this.shouldDescribeChange(evt)) {
    this.speak(Msgs.getMsg('text_box_blank'), true);
    if (this.brailleHandler_) {
      this.brailleHandler_.changed('' /*line*/, 0 /*start*/, 0 /*end*/,
                                   true /*multiline*/, null /*element*/,
                                   evt.start /*lineStart*/);
    }
  } else {
    goog.base(this, 'changed', evt);
  }
};


/** @override */
cvox.ChromeVoxEditableContentEditable.prototype.moveCursorToNextCharacter =
    function() {
  window.getSelection().modify('move', 'forward', 'character');
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableContentEditable.prototype.moveCursorToPreviousCharacter =
    function() {
  window.getSelection().modify('move', 'backward', 'character');
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/** @override */
cvox.ChromeVoxEditableContentEditable.prototype.moveCursorToNextParagraph =
    function() {
  window.getSelection().modify('move', 'forward', 'paragraph');
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};

/** @override */
cvox.ChromeVoxEditableContentEditable.prototype.moveCursorToPreviousParagraph =
    function() {
  window.getSelection().modify('move', 'backward', 'paragraph');
  cvox.ChromeVoxEventWatcher.handleTextChanged(true);
  return true;
};


/**
 * @override
 */
cvox.ChromeVoxEditableContentEditable.prototype.shouldDescribeChange =
    function(evt) {
  var sel = window.getSelection();
  var cursor = new cvox.Cursor(sel.baseNode, sel.baseOffset, '');

  // This is a very specific work around because of our buggy content editable
  // support. Blank new lines are not captured in the line indexing data
  // structures.
  // Scenario: given a piece of text like:
  //
  // Some Title
  //
  // Description
  // Footer
  //
  // The new lines after Title are not traversed to by TraverseUtil. A root fix
  // would make changes there. However, considering the fickle nature of that
  // code, we specifically detect for new lines here.
  if (Math.abs(this.start - evt.start) != 1 &&
      this.start == this.end &&
      evt.start == evt.end &&
      sel.baseNode == sel.extentNode &&
      sel.baseOffset == sel.extentOffset &&
      sel.baseNode.nodeType == Node.ELEMENT_NODE &&
      sel.baseNode.querySelector('BR') &&
      cvox.TraverseUtil.forwardsChar(cursor, [], [])) {
    // This case detects if the range selection surrounds a new line,
    // but there is still content after the new line (like the example
    // above after "Title"). In these cases, we "pretend" we're the
    // last character so we speak "blank".
    return false;
  }

  // Otherwise, we should never speak "blank" no matter what (even if
  // we're at the end of a content editable).
  return true;
};
