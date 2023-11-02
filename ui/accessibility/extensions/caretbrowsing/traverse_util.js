/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/**
 * @fileoverview Low-level DOM traversal utility functions to find the
 *     next (or previous) character, word, sentence, line, or paragraph,
 *     in a completely stateless manner without actually manipulating the
 *     selection.
 */

/**
 * A class to represent a cursor location in the document,
 * like the start position or end position of a selection range.
 *
 * Later this may be extended to support "virtual text" for an object,
 * like the ALT text for an image.
 *
 * Note: we cache the text of a particular node at the time we
 * traverse into it. Later we should add support for dynamically
 * reloading it.
 * @param {Node} node The DOM node.
 * @param {number} index The index of the character within the node.
 * @param {string} text The cached text contents of the node.
 * @constructor
 */
Cursor = function(node, index, text) {
  this.node = node;
  this.index = index;
  this.text = text;
};

/**
 * @return {Cursor} A new cursor pointing to the same location.
 */
Cursor.prototype.clone = function() {
  return new Cursor(this.node, this.index, this.text);
};

/**
 * Modify this cursor to point to the location that another cursor points to.
 * @param {Cursor} otherCursor The cursor to copy from.
 */
Cursor.prototype.copyFrom = function(otherCursor) {
  this.node = otherCursor.node;
  this.index = otherCursor.index;
  this.text = otherCursor.text;
};

/**
 * Utility functions for stateless DOM traversal.
 * @constructor
 */
TraverseUtil = function() {};

/**
 * Gets the text representation of a node. This allows us to substitute
 * alt text, names, or titles for html elements that provide them.
 * @param {Node} node A DOM node.
 * @return {string} A text string representation of the node.
 */
TraverseUtil.getNodeText = function(node) {
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
TraverseUtil.treatAsLeafNode = function(node) {
  return node.childNodes.length == 0 ||
         node.nodeName == 'SELECT' ||
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
TraverseUtil.isWhitespace = function(c) {
  return (c == ' ' || c == '\n' || c == '\r' || c == '\t');
};

/**
 * Set the selection to the range between the given start and end cursors.
 * @param {Cursor} start The desired start of the selection.
 * @param {Cursor} end The desired end of the selection.
 * @return {Selection} the selection object.
 */
TraverseUtil.setSelection = function(start, end) {
  const sel = window.getSelection();
  sel.removeAllRanges();
  const range = document.createRange();
  range.setStart(start.node, start.index);
  range.setEnd(end.node, end.index);
  sel.addRange(range);

  return sel;
};

/**
 * Use the computed CSS style to figure out if this DOM node is currently
 * visible.
 * @param {Node} node A HTML DOM node.
 * @return {boolean} Whether or not the html node is visible.
 */
TraverseUtil.isVisible = function(node) {
  if (!node.style)
    return true;
  const style = window.getComputedStyle(/** @type {Element} */(node), null);
  return (!!style && style.display != 'none' && style.visibility != 'hidden');
};

/**
 * Use the class name to figure out if this DOM node should be traversed.
 * @param {Node} node A HTML DOM node.
 * @return {boolean} Whether or not the html node should be traversed.
 */
TraverseUtil.isSkipped = function(node) {
  if (node.constructor == Text)
    node = node.parentElement;
  if (node.className == 'CaretBrowsing_Caret' ||
      node.className == 'CaretBrowsing_AnimateCaret') {
    return true;
  }
  return false;
};

/**
 * Moves the cursor forwards until it has crossed exactly one character.
 * @param {Cursor} cursor The cursor location where the search should start.
 *     On exit, the cursor will be immediately to the right of the
 *     character returned.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The character found, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.forwardsChar = function(cursor, nodesCrossed) {
  while (true) {
    // Move down until we get to a leaf node.
    let childNode = null;
    if (!TraverseUtil.treatAsLeafNode(cursor.node)) {
      for (let i = cursor.index; i < cursor.node.childNodes.length; i++) {
        const node = cursor.node.childNodes[i];
        if (TraverseUtil.isSkipped(node)) {
          nodesCrossed.push(node);
          continue;
        }
        if (TraverseUtil.isVisible(node)) {
          childNode = node;
          break;
        }
      }
    }
    if (childNode) {
      cursor.node = childNode;
      cursor.index = 0;
      cursor.text = TraverseUtil.getNodeText(cursor.node);
      if (cursor.node.constructor != Text) {
        nodesCrossed.push(cursor.node);
      }
      continue;
    }

    // Return the next character from this leaf node.
    if (cursor.index < cursor.text.length)
      return cursor.text[cursor.index++];

    // Move to the next sibling, going up the tree as necessary.
    while (cursor.node != null) {
      // Try to move to the next sibling.
      let siblingNode = null;
      for (let node = cursor.node.nextSibling;
           node != null;
           node = node.nextSibling) {
        if (TraverseUtil.isSkipped(node)) {
          nodesCrossed.push(node);
          continue;
        }
        if (TraverseUtil.isVisible(node)) {
          siblingNode = node;
          break;
        }
      }
      if (siblingNode) {
        cursor.node = siblingNode;
        cursor.text = TraverseUtil.getNodeText(siblingNode);
        cursor.index = 0;

        if (cursor.node.constructor != Text) {
          nodesCrossed.push(cursor.node);
        }

        break;
      }

      // Otherwise, move to the parent.
      if (cursor.node.parentNode &&
          cursor.node.parentNode.constructor != HTMLBodyElement) {
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
 * @param {Cursor} cursor The cursor location where the search should start.
 *     On exit, the cursor will be immediately to the left of the
 *     character returned.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The previous character, or null if the top of the
 *     document has been reached.
 */
TraverseUtil.backwardsChar = function(cursor, nodesCrossed) {
  while (true) {
    // Move down until we get to a leaf node.
    let childNode = null;
    if (!TraverseUtil.treatAsLeafNode(cursor.node)) {
      for (let i = cursor.index - 1; i >= 0; i--) {
        const node = cursor.node.childNodes[i];
        if (TraverseUtil.isSkipped(node)) {
          nodesCrossed.push(node);
          continue;
        }
        if (TraverseUtil.isVisible(node)) {
          childNode = node;
          break;
        }
      }
    }
    if (childNode) {
      cursor.node = childNode;
      cursor.text = TraverseUtil.getNodeText(cursor.node);
      if (cursor.text.length)
        cursor.index = cursor.text.length;
      else
        cursor.index = cursor.node.childNodes.length;
      if (cursor.node.constructor != Text)
        nodesCrossed.push(cursor.node);
      continue;
    }

    // Return the previous character from this leaf node.
    if (cursor.text.length > 0 && cursor.index > 0) {
      return cursor.text[--cursor.index];
    }

    // Move to the previous sibling, going up the tree as necessary.
    while (true) {
      // Try to move to the previous sibling.
      let siblingNode = null;
      for (let node = cursor.node.previousSibling;
           node != null;
           node = node.previousSibling) {
        if (TraverseUtil.isSkipped(node)) {
          nodesCrossed.push(node);
          continue;
        }
        if (TraverseUtil.isVisible(node)) {
          siblingNode = node;
          break;
        }
      }
      if (siblingNode) {
        cursor.node = siblingNode;
        cursor.text = TraverseUtil.getNodeText(siblingNode);
        if (cursor.text.length)
          cursor.index = cursor.text.length;
        else
          cursor.index = cursor.node.childNodes.length;
        if (cursor.node.constructor != Text)
          nodesCrossed.push(cursor.node);
        break;
      }

      // Otherwise, move to the parent.
      if (cursor.node.parentNode &&
          cursor.node.parentNode.constructor != HTMLBodyElement) {
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
 * @param {Cursor} startCursor On exit, points to the position before
 *     the char.
 * @param {Cursor} endCursor The position to start searching for the next
 *     char.  On exit, will point to the position past the char.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {boolean} skipWhitespace If true, will keep scanning until a
 *     non-whitespace character is found.
 * @return {?string} The next char, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextChar = function(
    startCursor, endCursor, nodesCrossed, skipWhitespace) {

  // Save the starting position and get the first character.
  startCursor.copyFrom(endCursor);
  let c = TraverseUtil.forwardsChar(endCursor, nodesCrossed);
  if (c == null)
    return null;

  // Keep track of whether the first character was whitespace.
  const initialWhitespace = TraverseUtil.isWhitespace(c);

  // Keep scanning until we find a non-whitespace or non-skipped character.
  while ((TraverseUtil.isWhitespace(c)) ||
      (TraverseUtil.isSkipped(endCursor.node))) {
    c = TraverseUtil.forwardsChar(endCursor, nodesCrossed);
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
    for (let i = 0; i < nodesCrossed.length; i++) {
      if (TraverseUtil.isSkipped(nodesCrossed[i])) {
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
 * @param {Cursor} startCursor The position to start searching for the
 *     char. On exit, will point to the position before the char.
 * @param {Cursor} endCursor The position to start searching for the next
 *     char. On exit, will point to the position past the char.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {boolean} skipWhitespace If true, will keep scanning until a
 *     non-whitespace character is found.
 * @return {?string} The previous char, or null if the top of the
 *     document has been reached.
 */
TraverseUtil.getPreviousChar = function(
    startCursor, endCursor, nodesCrossed, skipWhitespace) {

  // Save the starting position and get the first character.
  endCursor.copyFrom(startCursor);
  let c = TraverseUtil.backwardsChar(startCursor, nodesCrossed);
  if (c == null)
    return null;

  // Keep track of whether the first character was whitespace.
  const initialWhitespace = TraverseUtil.isWhitespace(c);

  // Keep scanning until we find a non-whitespace or non-skipped character.
  while ((TraverseUtil.isWhitespace(c)) ||
      (TraverseUtil.isSkipped(startCursor.node))) {
    c = TraverseUtil.backwardsChar(startCursor, nodesCrossed);
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
    for (let i = 0; i < nodesCrossed.length; i++) {
      if (TraverseUtil.isSkipped(nodesCrossed[i])) {
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
 * @param {Cursor} startCursor On exit, will point to the beginning of the
 *     word returned.
 * @param {Cursor} endCursor The position to start searching for the next
 *     word.  On exit, will point to the end of the word returned.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The next word, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextWord = function(startCursor, endCursor,
    nodesCrossed) {

  // Find the first non-whitespace or non-skipped character.
  const cursor = endCursor.clone();
  let c = TraverseUtil.forwardsChar(cursor, nodesCrossed);
  if (c == null)
    return null;
  while ((TraverseUtil.isWhitespace(c)) ||
      (TraverseUtil.isSkipped(cursor.node))) {
    c = TraverseUtil.forwardsChar(cursor, nodesCrossed);
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
  let word = c;
  const newNodesCrossed = [];
  c = TraverseUtil.forwardsChar(cursor, newNodesCrossed);
  if (c == null) {
    return word;
  }
  while (!TraverseUtil.isWhitespace(c) &&
     newNodesCrossed.length == 0) {
    word += c;
    endCursor.copyFrom(cursor);
    c = TraverseUtil.forwardsChar(cursor, newNodesCrossed);
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
 * @param {Cursor} startCursor The position to start searching for the
 *     previous word.  On exit, will point to the beginning of the
 *     word returned.
 * @param {Cursor} endCursor On exit, will point to the end of the
 *     word returned.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The previous word, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getPreviousWord = function(startCursor, endCursor,
    nodesCrossed) {
  // Find the first non-whitespace or non-skipped character.
  const cursor = startCursor.clone();
  let c = TraverseUtil.backwardsChar(cursor, nodesCrossed);
  if (c == null)
    return null;
  while ((TraverseUtil.isWhitespace(c) ||
      (TraverseUtil.isSkipped(cursor.node)))) {
    c = TraverseUtil.backwardsChar(cursor, nodesCrossed);
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
  let word = c;
  const newNodesCrossed = [];
  c = TraverseUtil.backwardsChar(cursor, newNodesCrossed);
  if (c == null)
    return word;
  while (!TraverseUtil.isWhitespace(c) &&
      newNodesCrossed.length == 0) {
    word = c + word;
    startCursor.copyFrom(cursor);
    c = TraverseUtil.backwardsChar(cursor, newNodesCrossed);
    if (c == null)
      return word;
  }

  return word;
};

/**
 * Finds the next sentence, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next sentence.
 *
 * @param {Cursor} startCursor On exit, marks the beginning of the sentence.
 * @param {Cursor} endCursor The position to start searching for the next
 *     sentence.  On exit, will point to the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {Object} breakTags Associative array of tags that should break
 *     the sentence.
 * @return {?string} The next sentence, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextSentence = function(
    startCursor, endCursor, nodesCrossed, breakTags) {
  return TraverseUtil.getNextString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        if (str.substr(-1) == '.')
          return true;
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
          if (style && (style.display != 'inline' ||
                        breakTags[nodes[i].tagName])) {
            return true;
          }
        }
        return false;
      });
};

/**
 * Finds the previous sentence, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous sentence.
 *
 * @param {Cursor} startCursor The position to start searching for the next
 *     sentence.  On exit, will point to the start of the returned string.
 * @param {Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {Object} breakTags Associative array of tags that should break
 *     the sentence.
 * @return {?string} The previous sentence, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getPreviousSentence = function(
    startCursor, endCursor, nodesCrossed, breakTags) {
  return TraverseUtil.getPreviousString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        if (word.substr(-1) == '.')
          return true;
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
          if (style && (style.display != 'inline' ||
                        breakTags[nodes[i].tagName])) {
            return true;
          }
        }
        return false;
      });
};

/**
 * Finds the next line, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next line.
 *
 * @param {Cursor} startCursor On exit, marks the beginning of the line.
 * @param {Cursor} endCursor The position to start searching for the next
 *     line.  On exit, will point to the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {number} lineLength The maximum number of characters in a line.
 * @param {Object} breakTags Associative array of tags that should break
 *     the line.
 * @return {?string} The next line, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextLine = function(
    startCursor, endCursor, nodesCrossed, lineLength, breakTags) {
  return TraverseUtil.getNextString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        if (str.length + word.length + 1 > lineLength)
          return true;
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
          if (style && (style.display != 'inline' ||
                        breakTags[nodes[i].tagName])) {
            return true;
          }
        }
        return false;
      });
};

/**
 * Finds the previous line, starting from startCursor.  Upon exit,
 * startCursor and endCursor will surround the previous line.
 *
 * @param {Cursor} startCursor The position to start searching for the next
 *     line.  On exit, will point to the start of the returned string.
 * @param {Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {number} lineLength The maximum number of characters in a line.
 * @param {Object} breakTags Associative array of tags that should break
 *     the sentence.
 *  @return {?string} The previous line, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getPreviousLine = function(
    startCursor, endCursor, nodesCrossed, lineLength, breakTags) {
  return TraverseUtil.getPreviousString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        if (str.length + word.length + 1 > lineLength)
          return true;
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
          if (style && (style.display != 'inline' ||
                        breakTags[nodes[i].tagName])) {
            return true;
          }
        }
        return false;
      });
};

/**
 * Finds the next paragraph, starting from endCursor.  Upon exit,
 * startCursor and endCursor will surround the next paragraph.
 *
 * @param {Cursor} startCursor On exit, marks the beginning of the paragraph.
 * @param {Cursor} endCursor The position to start searching for the next
 *     paragraph.  On exit, will point to the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The next paragraph, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextParagraph = function(startCursor, endCursor,
    nodesCrossed) {
  return TraverseUtil.getNextString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
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
 * @param {Cursor} startCursor The position to start searching for the next
 *     paragraph.  On exit, will point to the start of the returned string.
 * @param {Cursor} endCursor On exit, the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @return {?string} The previous paragraph, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getPreviousParagraph = function(
    startCursor, endCursor, nodesCrossed) {
  return TraverseUtil.getPreviousString(
      startCursor, endCursor, nodesCrossed,
      function(str, word, nodes) {
        for (let i = 0; i < nodes.length; i++) {
          if (TraverseUtil.isSkipped(nodes[i])) {
            return true;
          }
          const style = window.getComputedStyle(nodes[i], null);
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
 * The breakBefore function takes three parameters, and
 * should return true if the string should be broken before the proposed
 * next word:
 *   str The string so far.
 *   word The next word to be added.
 *   nodesCrossed The nodes crossed in reaching this next word.
 *
 * @param {Cursor} startCursor On exit, will point to the beginning of the
 *     next string.
 * @param {Cursor} endCursor The position to start searching for the next
 *     string.  On exit, will point to the end of the returned string.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {function(string, string, Array<string>)} breakBefore
 *     Function that takes the string so far, next word to be added, and
 *     nodes crossed, and returns true if the string should be ended before
 *     adding this word.
 * @return {?string} The next string, or null if the bottom of the
 *     document has been reached.
 */
TraverseUtil.getNextString = function(
    startCursor, endCursor, nodesCrossed, breakBefore) {
  // Get the first word and set the start cursor to the start of the
  // first word.
  const wordStartCursor = endCursor.clone();
  const wordEndCursor = endCursor.clone();
  let newNodesCrossed = [];
  let str = '';
  let word = TraverseUtil.getNextWord(
      wordStartCursor, wordEndCursor, newNodesCrossed);
  if (word == null)
    return null;
  startCursor.copyFrom(wordStartCursor);

  // Always add the first word when the string is empty, and then keep
  // adding more words as long as breakBefore returns false
  while (!str || !breakBefore(str, word, newNodesCrossed)) {
    // Append this word, set the end cursor to the end of this word, and
    // update the returned list of nodes crossed to include ones we crossed
    // in reaching this word.
    if (str)
      str += ' ';
    str += word;
    nodesCrossed = nodesCrossed.concat(newNodesCrossed);
    endCursor.copyFrom(wordEndCursor);

    // Get the next word and go back to the top of the loop.
    newNodesCrossed = [];
    word = TraverseUtil.getNextWord(
        wordStartCursor, wordEndCursor, newNodesCrossed);
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
 * @param {Cursor} startCursor The position to start searching for the
 *     previous string.  On exit, will point to the beginning of the
 *     string returned.
 * @param {Cursor} endCursor On exit, will point to the end of the
 *     string returned.
 * @param {Array<Node>} nodesCrossed Any HTML nodes crossed between the
 *     initial and final cursor position will be pushed onto this array.
 * @param {function(string, string, Array<string>)} breakBefore
 *     Function that takes the string so far, the word to be added, and
 *     nodes crossed, and returns true if the string should be ended before
 *     adding this word.
 * @return {?string} The next string, or null if the top of the
 *     document has been reached.
 */
TraverseUtil.getPreviousString = function(
    startCursor, endCursor, nodesCrossed, breakBefore) {
  // Get the first word and set the end cursor to the end of the
  // first word.
  const wordStartCursor = startCursor.clone();
  const wordEndCursor = startCursor.clone();
  let newNodesCrossed = [];
  let str = '';
  let word = TraverseUtil.getPreviousWord(
      wordStartCursor, wordEndCursor, newNodesCrossed);
  if (word == null)
    return null;
  endCursor.copyFrom(wordEndCursor);

  // Always add the first word when the string is empty, and then keep
  // adding more words as long as breakBefore returns false
  while (!str || !breakBefore(str, word, newNodesCrossed)) {
    // Prepend this word, set the start cursor to the start of this word, and
    // update the returned list of nodes crossed to include ones we crossed
    // in reaching this word.
    if (str)
      str = ' ' + str;
    str = word + str;
    nodesCrossed = nodesCrossed.concat(newNodesCrossed);
    startCursor.copyFrom(wordStartCursor);

    // Get the previous word and go back to the top of the loop.
    newNodesCrossed = [];
    word = TraverseUtil.getPreviousWord(
        wordStartCursor, wordEndCursor, newNodesCrossed);
    if (word == null)
      return str;
  }

  return str;
};
