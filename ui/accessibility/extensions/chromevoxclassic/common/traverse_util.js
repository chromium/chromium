// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Low-level DOM traversal utility functions to find the
 *     next (or previous) character, word, sentence, line, or paragraph,
 *     in a completely stateless manner without actually manipulating the
 *     selection.
 */

goog.provide('cvox.TraverseUtil');

goog.require('cvox.Cursor');
goog.require('cvox.DomPredicates');
goog.require('cvox.DomUtil');

/**
 * Utility functions for stateless DOM traversal.
 * @constructor
 */
cvox.TraverseUtil = function() {};

/**
 * Gets the text representation of a node. This allows us to substitute
 * alt text, names, or titles for html elements that provide them.
 * @param {Node} node A DOM node.
 * @return {string} A text string representation of the node.
 */
cvox.TraverseUtil.getNodeText = function(node) {
  if (node.constructor == Text) {
    return node.data;
  } else {
    return '';
  }
};

/**
 * Return true if a node should be treated as a leaf node, because
 * its children are properties of the object that shouldn't be traversed.
 *
 * TODO(dmazzoni): replace this with a predicate that detects nodes with
 * ARIA roles and other objects that have their own description.
 * For now we just detect a couple of common cases.
 *
 * @param {Node} node A DOM node.
 * @return {boolean} True if the node should be treated as a leaf node.
 */
cvox.TraverseUtil.treatAsLeafNode = function(node) {
  return node.childNodes.length == 0 ||
         node.nodeName == 'SELECT' ||
         node.getAttribute('role') == 'listbox' ||
         node.nodeName == 'OBJECT';
};

/**
 * Return true only if a single character is whitespace.
 * From https://developer.mozilla.org/en/Whitespace_in_the_DOM,
 * whitespace is defined as one of the characters
 *  "\t" TAB \u0009
 *  "\n" LF  \u000A
 *  "\r" CR  \u000D
 *  " "  SPC \u0020.
 *
 * @param {string} c A string containing a single character.
 * @return {boolean} True if the character is whitespace, otherwise false.
 */
cvox.TraverseUtil.isWhitespace = function(c) {
  return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
};

/**
 * Set the selection to the range between the given start and end cursors.
 * @param {cvox.Cursor} start The desired start of the selection.
 * @param {cvox.Cursor} end The desired end of the selection.
 * @return {Selection} the selection object.
 */
cvox.TraverseUtil.setSelection = function(start, end) {
  var sel = window.getSelection();
  sel.removeAllRanges();
  var range = document.createRange();
  range.setStart(start.node, start.index);
  range.setEnd(end.node, end.index);
  sel.addRange(range);

  return sel;
};

// TODO(dtseng): Combine with cvox.DomUtil.hasContent.
/**
 * Check if this DOM node has the attribute aria-hidden='true', which should
 * hide it from screen readers.
 * @param {Node} node An HTML DOM node.
 * @return {boolean} Whether or not the html node should be traversed.
 */
cvox.TraverseUtil.isHidden = function(node) {
  if (node instanceof HTMLElement &&
      node.getAttribute('aria-hidden') == 'true') {
    return true;
  }
  switch (node.tagName) {
    case 'SCRIPT':
    case 'NOSCRIPT':
      return true;
  }
  return false;
};

/**
 * Moves the cursor forwards until it has crossed exactly one character.
 * @param {cvox.Cursor} cursor The cursor location where the search should
 *     start.  On exit, the cursor will be immediately to the right of the
 *     character returned.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The character found, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.forwardsChar = function(
    cursor, elementsEntered, elementsLeft) {
  while (true) {
    // Move down until we get to a leaf node.
    var childNode = null;
    if (!cvox.TraverseUtil.treatAsLeafNode(cursor.node)) {
      for (var i = cursor.index; i < cursor.node.childNodes.length; i++) {
        var node = cursor.node.childNodes[i];
        if (cvox.TraverseUtil.isHidden(node)) {
          if (node instanceof HTMLElement) {
            elementsEntered.push(node);
          }
          continue;
        }
        if (cvox.DomUtil.isVisible(node, {checkAncestors: false})) {
          childNode = node;
          break;
        }
      }
    }
    if (childNode) {
      cursor.node = childNode;
      cursor.index = 0;
      cursor.text = cvox.TraverseUtil.getNodeText(cursor.node);
      if (cursor.node instanceof HTMLElement) {
        elementsEntered.push(cursor.node);
      }
      continue;
    }

    // Return the next character from this leaf node.
    if (cursor.index < cursor.text.length)
      return cursor.text[cursor.index++];

    // Move to the next sibling, going up the tree as necessary.
    while (cursor.node != null) {
      // Try to move to the next sibling.
      var siblingNode = null;
      for (var node = cursor.node.nextSibling;
           node != null;
           node = node.nextSibling) {
        if (cvox.TraverseUtil.isHidden(node)) {
          if (node instanceof HTMLElement) {
            elementsEntered.push(node);
          }
          continue;
        }
        if (cvox.DomUtil.isVisible(node, {checkAncestors: false})) {
          siblingNode = node;
          break;
        }
      }
      if (siblingNode) {
        if (cursor.node instanceof HTMLElement) {
          elementsLeft.push(cursor.node);
        }

        cursor.node = siblingNode;
        cursor.text = cvox.TraverseUtil.getNodeText(siblingNode);
        cursor.index = 0;

        if (cursor.node instanceof HTMLElement) {
          elementsEntered.push(cursor.node);
        }

        break;
      }

      // Otherwise, move to the parent.
      if (cursor.node.parentNode &&
          cursor.node.parentNode.constructor != HTMLBodyElement) {
        if (cursor.node instanceof HTMLElement) {
          elementsLeft.push(cursor.node);
        }
        cursor.node = cursor.node.parentNode;
        cursor.text = null;
        cursor.index = 0;
      } else {
        return null;
      }
    }
  }
};

/**
 * Moves the cursor backwards until it has crossed exactly one character.
 * @param {cvox.Cursor} cursor The cursor location where the search should
 *     start.  On exit, the cursor will be immediately to the left of the
 *     character returned.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The previous character, or null if the top of the
 *     document has been reached.
 */
cvox.TraverseUtil.backwardsChar = function(
    cursor, elementsEntered, elementsLeft) {
  while (true) {
    // Move down until we get to a leaf node.
    var childNode = null;
    if (!cvox.TraverseUtil.treatAsLeafNode(cursor.node)) {
      for (var i = cursor.index - 1; i >= 0; i--) {
        var node = cursor.node.childNodes[i];
        if (cvox.TraverseUtil.isHidden(node)) {
          if (node instanceof HTMLElement) {
            elementsEntered.push(node);
          }
          continue;
        }
        if (cvox.DomUtil.isVisible(node, {checkAncestors: false})) {
          childNode = node;
          break;
        }
      }
    }
    if (childNode) {
      cursor.node = childNode;
      cursor.text = cvox.TraverseUtil.getNodeText(cursor.node);
      if (cursor.text.length)
        cursor.index = cursor.text.length;
      else
        cursor.index = cursor.node.childNodes.length;
      if (cursor.node instanceof HTMLElement) {
        elementsEntered.push(cursor.node);
      }
      continue;
    }

    // Return the previous character from this leaf node.
    if (cursor.text.length > 0 && cursor.index > 0) {
      return cursor.text[--cursor.index];
    }

    // Move to the previous sibling, going up the tree as necessary.
    while (true) {
      // Try to move to the previous sibling.
      var siblingNode = null;
      for (var node = cursor.node.previousSibling;
           node != null;
           node = node.previousSibling) {
        if (cvox.TraverseUtil.isHidden(node)) {
          if (node instanceof HTMLElement) {
            elementsEntered.push(node);
          }
          continue;
        }
        if (cvox.DomUtil.isVisible(node, {checkAncestors: false})) {
          siblingNode = node;
          break;
        }
      }
      if (siblingNode) {
        if (cursor.node instanceof HTMLElement) {
          elementsLeft.push(cursor.node);
        }

        cursor.node = siblingNode;
        cursor.text = cvox.TraverseUtil.getNodeText(siblingNode);
        if (cursor.text.length)
          cursor.index = cursor.text.length;
        else
          cursor.index = cursor.node.childNodes.length;

        if (cursor.node instanceof HTMLElement) {
          elementsEntered.push(cursor.node);
        }
        break;
      }

      // Otherwise, move to the parent.
      if (cursor.node.parentNode &&
          cursor.node.parentNode.constructor != HTMLBodyElement) {
        if (cursor.node instanceof HTMLElement) {
          elementsLeft.push(cursor.node);
        }
        cursor.node = cursor.node.parentNode;
        cursor.text = null;
        cursor.index = 0;
      } else {
        return null;
      }
    }
  }
};

/**
 * Finds the next character, starting from endCursor.  Upon exit, startCursor
 * and endCursor will surround the next character. If skipWhitespace is
 * true, will skip until a real character is found. Otherwise, it will
 * attempt to select all of the whitespace between the initial position
 * of endCursor and the next non-whitespace character.
 * @param {!cvox.Cursor} startCursor On exit, points to the position before
 *     the char.
 * @param {!cvox.Cursor} endCursor The position to start searching for the next
 *     char.  On exit, will point to the position past the char.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 *     initial and final cursor position will be pushed onto this array.
 * @param {boolean} skipWhitespace If true, will keep scanning until a
 *     non-whitespace character is found.
 * @return {?string} The next char, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextChar = function(
    startCursor, endCursor, elementsEntered, elementsLeft, skipWhitespace) {

  // Save the starting position and get the first character.
  startCursor.copyFrom(endCursor);
  var c = cvox.TraverseUtil.forwardsChar(
      endCursor, elementsEntered, elementsLeft);
  if (c == null)
    return null;

  // Keep track of whether the first character was whitespace.
  var initialWhitespace = cvox.TraverseUtil.isWhitespace(c);

  // Keep scanning until we find a non-whitespace or non-skipped character.
  while ((cvox.TraverseUtil.isWhitespace(c)) ||
      (cvox.TraverseUtil.isHidden(endCursor.node))) {
    c = cvox.TraverseUtil.forwardsChar(
        endCursor, elementsEntered, elementsLeft);
    if (c == null)
      return null;
  }
  if (skipWhitespace || !initialWhitespace) {
    // If skipWhitepace is true, or if the first character we encountered
    // was not whitespace, return that non-whitespace character.
    startCursor.copyFrom(endCursor);
    startCursor.index--;
    return c;
  }
  else {
    for (var i = 0; i < elementsEntered.length; i++) {
      if (cvox.TraverseUtil.isHidden(elementsEntered[i])) {
        // We need to make sure that startCursor and endCursor aren't
        // surrounding a skippable node.
        endCursor.index--;
        startCursor.copyFrom(endCursor);
        startCursor.index--;
        return ' ';
      }
    }
    // Otherwise, return all of the whitespace before that last character.
    endCursor.index--;
    return ' ';
  }
};

/**
 * Finds the previous character, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous character.
 * If skipWhitespace is true, will skip until a real character is found.
 * Otherwise, it will attempt to select all of the whitespace between
 * the initial position of endCursor and the next non-whitespace character.
 * @param {!cvox.Cursor} startCursor The position to start searching for the
 *     char. On exit, will point to the position before the char.
 * @param {!cvox.Cursor} endCursor The position to start searching for the next
 *     char. On exit, will point to the position past the char.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 *     initial and final cursor position will be pushed onto this array.
 * @param {boolean} skipWhitespace If true, will keep scanning until a
 *     non-whitespace character is found.
 * @return {?string} The previous char, or null if the top of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousChar = function(
    startCursor, endCursor, elementsEntered, elementsLeft, skipWhitespace) {

  // Save the starting position and get the first character.
  endCursor.copyFrom(startCursor);
  var c = cvox.TraverseUtil.backwardsChar(
      startCursor, elementsEntered, elementsLeft);
  if (c == null)
    return null;

  // Keep track of whether the first character was whitespace.
  var initialWhitespace = cvox.TraverseUtil.isWhitespace(c);

  // Keep scanning until we find a non-whitespace or non-skipped character.
  while ((cvox.TraverseUtil.isWhitespace(c)) ||
      (cvox.TraverseUtil.isHidden(startCursor.node))) {
    c = cvox.TraverseUtil.backwardsChar(
        startCursor, elementsEntered, elementsLeft);
    if (c == null)
      return null;
  }
  if (skipWhitespace || !initialWhitespace) {
    // If skipWhitepace is true, or if the first character we encountered
    // was not whitespace, return that non-whitespace character.
    endCursor.copyFrom(startCursor);
    endCursor.index++;
    return c;
  } else {
    for (var i = 0; i < elementsEntered.length; i++) {
      if (cvox.TraverseUtil.isHidden(elementsEntered[i])) {
        startCursor.index++;
        endCursor.copyFrom(startCursor);
        endCursor.index++;
        return ' ';
      }
    }
    // Otherwise, return all of the whitespace before that last character.
    startCursor.index++;
    return ' ';
  }
};

/**
 * Finds the next word, starting from endCursor.  Upon exit, startCursor
 * and endCursor will surround the next word.  A word is defined to be
 * a string of 1 or more non-whitespace characters in the same DOM node.
 * @param {cvox.Cursor} startCursor On exit, will point to the beginning of the
 *     word returned.
 * @param {cvox.Cursor} endCursor The position to start searching for the next
 *     word.  On exit, will point to the end of the word returned.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The next word, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextWord = function(startCursor, endCursor,
    elementsEntered, elementsLeft) {

  // Find the first non-whitespace or non-skipped character.
  var cursor = endCursor.clone();
  var c = cvox.TraverseUtil.forwardsChar(cursor, elementsEntered, elementsLeft);
  if (c == null)
    return null;
  while ((cvox.TraverseUtil.isWhitespace(c)) ||
      (cvox.TraverseUtil.isHidden(cursor.node))) {
    c = cvox.TraverseUtil.forwardsChar(cursor, elementsEntered, elementsLeft);
    if (c == null)
      return null;
  }

  // Set startCursor to the position immediately before the first
  // character in our word. It's safe to decrement |index| because
  // forwardsChar guarantees that the cursor will be immediately to the
  // right of the returned character on exit.
  startCursor.copyFrom(cursor);
  startCursor.index--;

  // Keep building up our word until we reach a whitespace character or
  // would cross a tag.  Don't actually return any tags crossed, because this
  // word goes up until the tag boundary but not past it.
  endCursor.copyFrom(cursor);
  var word = c;
  var newEntered = [];
  var newLeft = [];
  c = cvox.TraverseUtil.forwardsChar(cursor, newEntered, newLeft);
  if (c == null) {
    return word;
  }
  while (!cvox.TraverseUtil.isWhitespace(c) &&
         newEntered.length == 0 &&
         newLeft == 0) {
    word += c;
    endCursor.copyFrom(cursor);
    c = cvox.TraverseUtil.forwardsChar(cursor, newEntered, newLeft);
    if (c == null) {
      return word;
    }
  }

  return word;
};

/**
 * Finds the previous word, starting from startCursor.  Upon exit, startCursor
 * and endCursor will surround the previous word.  A word is defined to be
 * a string of 1 or more non-whitespace characters in the same DOM node.
 * @param {cvox.Cursor} startCursor The position to start searching for the
 *     previous word.  On exit, will point to the beginning of the
 *     word returned.
 * @param {cvox.Cursor} endCursor On exit, will point to the end of the
 *     word returned.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The previous word, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousWord = function(startCursor, endCursor,
    elementsEntered, elementsLeft) {
  // Find the first non-whitespace or non-skipped character.
  var cursor = startCursor.clone();
  var c = cvox.TraverseUtil.backwardsChar(
      cursor, elementsEntered, elementsLeft);
  if (c == null)
    return null;
  while ((cvox.TraverseUtil.isWhitespace(c) ||
      (cvox.TraverseUtil.isHidden(cursor.node)))) {
    c = cvox.TraverseUtil.backwardsChar(cursor, elementsEntered, elementsLeft);
    if (c == null)
      return null;
  }

  // Set endCursor to the position immediately after the first
  // character we've found (the last character of the word, since we're
  // searching backwards).
  endCursor.copyFrom(cursor);
  endCursor.index++;

  // Keep building up our word until we reach a whitespace character or
  // would cross a tag.  Don't actually return any tags crossed, because this
  // word goes up until the tag boundary but not past it.
  startCursor.copyFrom(cursor);
  var word = c;
  var newEntered = [];
  var newLeft = [];
  c = cvox.TraverseUtil.backwardsChar(cursor, newEntered, newLeft);
  if (c == null)
    return word;
  while (!cvox.TraverseUtil.isWhitespace(c) &&
         newEntered.length == 0 &&
         newLeft.length == 0) {
    word = c + word;
    startCursor.copyFrom(cursor);

    c = cvox.TraverseUtil.backwardsChar(cursor, newEntered, newLeft);
    if (c == null)
      return word;
  }

  return word;
};


/**
 * Given elements entered and left, and break tags, returns true if the
 *     current word should break.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {Object<boolean>} breakTags Associative array of tags that should
 *     break.
 * @return {boolean} True if elementsEntered or elementsLeft include an
 *     element with one of these tags.
 */
cvox.TraverseUtil.includesBreakTagOrSkippedNode = function(
    elementsEntered, elementsLeft, breakTags) {
  for (var i = 0; i < elementsEntered.length; i++) {
    if (cvox.TraverseUtil.isHidden(elementsEntered[i])) {
      return true;
    }
    var style = window.getComputedStyle(elementsEntered[i], null);
    if ((style && style.display != 'inline') ||
        breakTags[elementsEntered[i].tagName]) {
      return true;
    }
  }
  for (i = 0; i < elementsLeft.length; i++) {
    var style = window.getComputedStyle(elementsLeft[i], null);
    if ((style && style.display != 'inline') ||
        breakTags[elementsLeft[i].tagName]) {
      return true;
    }
  }
  return false;
};


/**
 * Finds the next sentence, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next sentence.
 *
 * @param {cvox.Cursor} startCursor On exit, marks the beginning of the
 *     sentence.
 * @param {cvox.Cursor} endCursor The position to start searching for the next
 *     sentence.  On exit, will point to the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {Object<boolean>} breakTags Associative array of tags that should
 *     break the sentence.
 * @return {?string} The next sentence, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextSentence = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakTags) {
  return cvox.TraverseUtil.getNextString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        if (str.substr(-1) == '.')
          return true;
        return cvox.TraverseUtil.includesBreakTagOrSkippedNode(
            elementsEntered, elementsLeft, breakTags);
      });
};

/**
 * Finds the previous sentence, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous sentence.
 *
 * @param {cvox.Cursor} startCursor The position to start searching for the next
 *     sentence.  On exit, will point to the start of the returned string.
 * @param {cvox.Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {Object<boolean>} breakTags Associative array of tags that should
 *     break the sentence.
 * @return {?string} The previous sentence, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousSentence = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakTags) {
  return cvox.TraverseUtil.getPreviousString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        if (word.substr(-1) == '.')
          return true;
        return cvox.TraverseUtil.includesBreakTagOrSkippedNode(
            elementsEntered, elementsLeft, breakTags);
      });
};

/**
 * Finds the next line, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next line.
 *
 * @param {cvox.Cursor} startCursor On exit, marks the beginning of the line.
 * @param {cvox.Cursor} endCursor The position to start searching for the next
 *     line.  On exit, will point to the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {Object<boolean>} breakTags Associative array of tags that should
 *     break the line.
 * @return {?string} The next line, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextLine = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakTags) {
  var range = document.createRange();
  var currentRect = null;
  var rightMostRect = null;
  var prevCursor = endCursor.clone();
 return cvox.TraverseUtil.getNextString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        range.setStart(startCursor.node, startCursor.index);
        range.setEnd(endCursor.node, endCursor.index);
        var currentRect = range.getBoundingClientRect();
        if (!rightMostRect) {
          rightMostRect = currentRect;
        }

        // Break at new lines except when within a link.
        if (currentRect.bottom != rightMostRect.bottom &&
            !cvox.DomPredicates.linkPredicate(cvox.DomUtil.getAncestors(
                endCursor.node))) {
          endCursor.copyFrom(prevCursor);
          return true;
        }

        rightMostRect = currentRect;
        prevCursor.copyFrom(endCursor);

        return cvox.TraverseUtil.includesBreakTagOrSkippedNode(
            elementsEntered, elementsLeft, breakTags);
      });
};

/**
 * Finds the previous line, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous line.
 *
 * @param {cvox.Cursor} startCursor The position to start searching for the next
 *     line.  On exit, will point to the start of the returned string.
 * @param {cvox.Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {Object<boolean>} breakTags Associative array of tags that should
 *     break the line.
 * @return {?string} The previous line, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousLine = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakTags) {
  var range = document.createRange();
  var currentRect = null;
  var leftMostRect = null;
  var prevCursor = startCursor.clone();
  return cvox.TraverseUtil.getPreviousString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        range.setStart(startCursor.node, startCursor.index);
        range.setEnd(endCursor.node, endCursor.index);
        var currentRect = range.getBoundingClientRect();
        if (!leftMostRect) {
          leftMostRect = currentRect;
        }

        // Break at new lines except when within a link.
        if (currentRect.top != leftMostRect.top &&
            !cvox.DomPredicates.linkPredicate(cvox.DomUtil.getAncestors(
                startCursor.node))) {
          startCursor.copyFrom(prevCursor);
          return true;
        }

        leftMostRect = currentRect;
        prevCursor.copyFrom(startCursor);

        return cvox.TraverseUtil.includesBreakTagOrSkippedNode(
            elementsEntered, elementsLeft, breakTags);
      });
};

/**
 * Finds the next paragraph, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next paragraph.
 *
 * @param {cvox.Cursor} startCursor On exit, marks the beginning of the
 *     paragraph.
 * @param {cvox.Cursor} endCursor The position to start searching for the next
 *     paragraph.  On exit, will point to the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The next paragraph, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextParagraph = function(startCursor, endCursor,
    elementsEntered, elementsLeft) {
  return cvox.TraverseUtil.getNextString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        for (var i = 0; i < elementsEntered.length; i++) {
          if (cvox.TraverseUtil.isHidden(elementsEntered[i])) {
            return true;
          }
          var style = window.getComputedStyle(elementsEntered[i], null);
          if (style && style.display != 'inline') {
            return true;
          }
        }
        for (i = 0; i < elementsLeft.length; i++) {
          var style = window.getComputedStyle(elementsLeft[i], null);
          if (style && style.display != 'inline') {
            return true;
          }
        }
        return false;
      });
};

/**
 * Finds the previous paragraph, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous paragraph.
 *
 * @param {cvox.Cursor} startCursor The position to start searching for the next
 *     paragraph.  On exit, will point to the start of the returned string.
 * @param {cvox.Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @return {?string} The previous paragraph, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousParagraph = function(
    startCursor, endCursor, elementsEntered, elementsLeft) {
  return cvox.TraverseUtil.getPreviousString(
      startCursor, endCursor, elementsEntered, elementsLeft,
      function(str, word, elementsEntered, elementsLeft) {
        for (var i = 0; i < elementsEntered.length; i++) {
          if (cvox.TraverseUtil.isHidden(elementsEntered[i])) {
            return true;
          }
          var style = window.getComputedStyle(elementsEntered[i], null);
          if (style && style.display != 'inline') {
            return true;
          }
        }
        for (i = 0; i < elementsLeft.length; i++) {
          var style = window.getComputedStyle(elementsLeft[i], null);
          if (style && style.display != 'inline') {
            return true;
          }
        }
        return false;
      });
};

/**
 * Customizable function to return the next string of words in the DOM, based
 * on provided functions to decide when to break one string and start
 * the next. This can be used to get the next sentence, line, paragraph,
 * or potentially other granularities.
 *
 * Finds the next contiguous string, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next string.
 *
 * The breakBefore function takes four parameters, and
 * should return true if the string should be broken before the proposed
 * next word:
 *   str The string so far.
 *   word The next word to be added.
 *   elementsEntered The elements entered in reaching this next word.
 *   elementsLeft The elements left in reaching this next word.
 *
 * @param {cvox.Cursor} startCursor On exit, will point to the beginning of the
 *     next string.
 * @param {cvox.Cursor} endCursor The position to start searching for the next
 *     string.  On exit, will point to the end of the returned string.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {function(string, string, Array<Element>, Array<Element>)}
 *     breakBefore Function that takes the string so far, next word to be
 *     added, and elements entered and left, and returns true if the string
 *     should be ended before adding this word.
 * @return {?string} The next string, or null if the bottom of the
 *     document has been reached.
 */
cvox.TraverseUtil.getNextString = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakBefore) {
  // Get the first word and set the start cursor to the start of the
  // first word.
  var wordStartCursor = endCursor.clone();
  var wordEndCursor = endCursor.clone();
  var newEntered = [];
  var newLeft = [];
  var str = '';
  var word = cvox.TraverseUtil.getNextWord(
      wordStartCursor, wordEndCursor, newEntered, newLeft);
  if (word == null)
    return null;
  startCursor.copyFrom(wordStartCursor);

  // Always add the first word when the string is empty, and then keep
  // adding more words as long as breakBefore returns false
  while (!str || !breakBefore(str, word, newEntered, newLeft)) {
    // Append this word, set the end cursor to the end of this word, and
    // update the returned list of nodes crossed to include ones we crossed
    // in reaching this word.
    if (str)
      str += ' ';
    str += word;
    elementsEntered = elementsEntered.concat(newEntered);
    elementsLeft = elementsLeft.concat(newLeft);
    endCursor.copyFrom(wordEndCursor);

    // Get the next word and go back to the top of the loop.
    newEntered = [];
    newLeft = [];
    word = cvox.TraverseUtil.getNextWord(
        wordStartCursor, wordEndCursor, newEntered, newLeft);
    if (word == null)
      return str;
  }

  return str;
};

/**
 * Customizable function to return the previous string of words in the DOM,
 * based on provided functions to decide when to break one string and start
 * the next. See getNextString, above, for more details.
 *
 * Finds the previous contiguous string, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the next string.
 *
 * @param {cvox.Cursor} startCursor The position to start searching for the
 *     previous string.  On exit, will point to the beginning of the
 *     string returned.
 * @param {cvox.Cursor} endCursor On exit, will point to the end of the
 *     string returned.
 * @param {Array<Element>} elementsEntered Any HTML elements entered.
 * @param {Array<Element>} elementsLeft Any HTML elements left.
 * @param {function(string, string, Array<Element>, Array<Element>)}
 *     breakBefore Function that takes the string so far, the word to be
 *     added, and nodes crossed, and returns true if the string should be
 *     ended before adding this word.
 * @return {?string} The next string, or null if the top of the
 *     document has been reached.
 */
cvox.TraverseUtil.getPreviousString = function(
    startCursor, endCursor, elementsEntered, elementsLeft, breakBefore) {
  // Get the first word and set the end cursor to the end of the
  // first word.
  var wordStartCursor = startCursor.clone();
  var wordEndCursor = startCursor.clone();
  var newEntered = [];
  var newLeft = [];
  var str = '';
  var word = cvox.TraverseUtil.getPreviousWord(
      wordStartCursor, wordEndCursor, newEntered, newLeft);
  if (word == null)
    return null;
  endCursor.copyFrom(wordEndCursor);

  // Always add the first word when the string is empty, and then keep
  // adding more words as long as breakBefore returns false
  while (!str || !breakBefore(str, word, newEntered, newLeft)) {
    // Prepend this word, set the start cursor to the start of this word, and
    // update the returned list of nodes crossed to include ones we crossed
    // in reaching this word.
    if (str)
      str = ' ' + str;
    str = word + str;
    elementsEntered = elementsEntered.concat(newEntered);
    elementsLeft = elementsLeft.concat(newLeft);
    startCursor.copyFrom(wordStartCursor);

    // Get the previous word and go back to the top of the loop.
    newEntered = [];
    newLeft = [];
    word = cvox.TraverseUtil.getPreviousWord(
        wordStartCursor, wordEndCursor, newEntered, newLeft);
    if (word == null)
      return str;
  }

  return str;
};
