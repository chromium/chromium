/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Plugin to handle enter keys.
 */

goog.provide('goog.editor.plugins.EnterHandler');

goog.require('goog.dom');
goog.require('goog.dom.NodeOffset');
goog.require('goog.dom.NodeType');
goog.require('goog.dom.Range');
goog.require('goog.dom.TagName');
goog.require('goog.editor.BrowserFeature');
goog.require('goog.editor.Plugin');
goog.require('goog.editor.node');
goog.require('goog.editor.plugins.Blockquote');
goog.require('goog.editor.range');
goog.require('goog.editor.style');
goog.require('goog.events.KeyCodes');
goog.require('goog.functions');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.userAgent');
goog.requireType('goog.dom.AbstractRange');
goog.requireType('goog.events.BrowserEvent');
goog.requireType('goog.events.Event');



/**
 * Plugin to handle enter keys. This does all the crazy to normalize (as much as
 * is reasonable) what happens when you hit enter. This also handles the
 * special casing of hitting enter in a blockquote.
 *
 * In IE, Webkit, and Opera, the resulting HTML uses one DIV tag per line. In
 * Firefox, the resulting HTML uses BR tags at the end of each line.
 *
 * @constructor
 * @extends {goog.editor.Plugin}
 */
goog.editor.plugins.EnterHandler = function() {
  'use strict';
  goog.editor.Plugin.call(this);
};
goog.inherits(goog.editor.plugins.EnterHandler, goog.editor.Plugin);


/**
 * The type of block level tag to add on enter, for browsers that support
 * specifying the default block-level tag. Can be overriden by subclasses; must
 * be either DIV or P.
 * @type {!goog.dom.TagName}
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.tag = goog.dom.TagName.DIV;


/** @override */
goog.editor.plugins.EnterHandler.prototype.getTrogClassId = function() {
  'use strict';
  return 'EnterHandler';
};


/** @override */
goog.editor.plugins.EnterHandler.prototype.enable = function(fieldObject) {
  'use strict';
  goog.editor.plugins.EnterHandler.base(this, 'enable', fieldObject);

  if (goog.editor.BrowserFeature.SUPPORTS_OPERA_DEFAULTBLOCK_COMMAND &&
      (this.tag == goog.dom.TagName.P || this.tag == goog.dom.TagName.DIV)) {
    var doc = this.getFieldDomHelper().getDocument();
    doc.execCommand('opera-defaultBlock', false, this.tag);
  }
};


/**
 * If the contents are empty, return the 'default' html for the field.
 * The 'default' contents depend on the enter handling mode, so it
 * makes the most sense in this plugin.
 * @param {string} html The html to prepare.
 * @return {string} The original HTML, or default contents if that
 *    html is empty.
 * @override
 */
goog.editor.plugins.EnterHandler.prototype.prepareContentsHtml = function(
    html) {
  'use strict';
  if (!html || goog.string.isBreakingWhitespace(html)) {
    return goog.editor.BrowserFeature.COLLAPSES_EMPTY_NODES ?
        this.getNonCollapsingBlankHtml() :
        '';
  }
  return html;
};


/**
 * Gets HTML with no contents that won't collapse, for browsers that
 * collapse the empty string.
 * @return {string} Blank html.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.getNonCollapsingBlankHtml =
    goog.functions.constant('<br>');


/**
 * Internal backspace handler.
 * @param {goog.events.Event} e The keypress event.
 * @param {goog.dom.AbstractRange} range The closure range object.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.handleBackspaceInternal = function(
    e, range) {
  'use strict';
  var field = this.getFieldObject().getElement();
  var container = range && range.getStartNode();

  if (field.firstChild == container && goog.editor.node.isEmpty(container)) {
    e.preventDefault();
    // TODO(user): I think we probably don't need to stopPropagation here
    e.stopPropagation();
  }
};


/**
 * Fix paragraphs to be the correct type of node.
 * @param {goog.events.Event} e The `<enter>` key event.
 * @param {boolean} split Whether we already split up a blockquote by
 *     manually inserting elements.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.processParagraphTagsInternal =
    function(e, split) {
  'use strict';
  // Force IE to turn the node we are leaving into a DIV.  If we do turn
  // it into a DIV, the node IE creates in response to ENTER will also be
  // a DIV.  If we don't, it will be a P.  We handle that case
  // in handleKeyUpIE_
  if (goog.userAgent.IE) {
    this.ensureBlockIeOpera(goog.dom.TagName.DIV);
  } else if (!split && goog.userAgent.WEBKIT) {
    // WebKit duplicates a blockquote when the user hits enter. Let's cancel
    // this and insert a BR instead, to make it more consistent with the other
    // browsers.
    var range = this.getFieldObject().getRange();
    if (!range ||
        !goog.editor.plugins.EnterHandler.isDirectlyInBlockquote(
            range.getContainerElement())) {
      return;
    }

    var dh = this.getFieldDomHelper();
    var br = dh.createElement(goog.dom.TagName.BR);
    range.insertNode(br, true);

    // If the BR is at the end of a block element, Safari still thinks there is
    // only one line instead of two, so we need to add another BR in that case.
    if (goog.editor.node.isBlockTag(br.parentNode) &&
        !goog.editor.node.skipEmptyTextNodes(br.nextSibling)) {
      goog.dom.insertSiblingBefore(dh.createElement(goog.dom.TagName.BR), br);
    }

    goog.editor.range.placeCursorNextTo(br, false);
    e.preventDefault();
  }
};


/**
 * Determines whether the lowest containing block node is a blockquote.
 * @param {Node} n The node.
 * @return {boolean} Whether the deepest block ancestor of n is a blockquote.
 */
goog.editor.plugins.EnterHandler.isDirectlyInBlockquote = function(n) {
  'use strict';
  for (var current = n; current; current = current.parentNode) {
    if (goog.editor.node.isBlockTag(current)) {
      return /** @type {!Element} */ (current).tagName ==
          goog.dom.TagName.BLOCKQUOTE;
    }
  }

  return false;
};


/**
 * Internal delete key handler.
 * @param {goog.events.Event} e The keypress event.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.handleDeleteGecko = function(e) {
  'use strict';
  this.deleteBrGecko(e);
};


/**
 * Deletes the element at the cursor if it is a BR node, and if it does, calls
 * e.preventDefault to stop the browser from deleting. Only necessary in Gecko
 * as a workaround for mozilla bug 205350 where deleting a BR that is followed
 * by a block element doesn't work (the BR gets immediately replaced). We also
 * need to account for an ill-formed cursor which occurs from us trying to
 * stop the browser from deleting.
 *
 * @param {goog.events.Event} e The DELETE keypress event.
 * @protected
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.editor.plugins.EnterHandler.prototype.deleteBrGecko = function(e) {
  'use strict';
  var range = this.getFieldObject().getRange();
  if (range.isCollapsed()) {
    var container = range.getEndNode();
    if (container.nodeType == goog.dom.NodeType.ELEMENT) {
      var nextNode = container.childNodes[range.getEndOffset()];
      if (nextNode && nextNode.tagName == goog.dom.TagName.BR) {
        // We want to retrieve the first non-whitespace previous sibling
        // as we could have added an empty text node below and want to
        // properly handle deleting a sequence of BR's.
        var previousSibling = goog.editor.node.getPreviousSibling(nextNode);
        var nextSibling = nextNode.nextSibling;

        container.removeChild(nextNode);
        e.preventDefault();

        // When we delete a BR followed by a block level element, the cursor
        // has a line-height which spans the height of the block level element.
        // e.g. If we delete a BR followed by a UL, the resulting HTML will
        // appear to the end user like:-
        //
        // |  * one
        // |  * two
        // |  * three
        //
        // There are a couple of cases that we have to account for in order to
        // properly conform to what the user expects when DELETE is pressed.
        //
        // 1. If the BR has a previous sibling and the previous sibling is
        //    not a block level element or a BR, we place the cursor at the
        //    end of that.
        // 2. If the BR doesn't have a previous sibling or the previous sibling
        //    is a block level element or a BR, we place the cursor at the
        //    beginning of the leftmost leaf of its next sibling.
        if (nextSibling && goog.editor.node.isBlockTag(nextSibling)) {
          if (previousSibling &&
              !(previousSibling.tagName == goog.dom.TagName.BR ||
                goog.editor.node.isBlockTag(previousSibling))) {
            goog.dom.Range
                .createCaret(
                    previousSibling,
                    goog.editor.node.getLength(previousSibling))
                .select();
          } else {
            var leftMostLeaf = goog.editor.node.getLeftMostLeaf(nextSibling);
            goog.dom.Range.createCaret(leftMostLeaf, 0).select();
          }
        }
      }
    }
  }
};


/** @override */
goog.editor.plugins.EnterHandler.prototype.handleKeyDown = function(e) {
  'use strict';
  if (goog.userAgent.GECKO) {
    // If a dialog doesn't have selectable field, Gecko grabs the event and
    // performs actions in editor window. This solves that problem and allows
    // the event to be passed on to proper handlers.
    if (this.getFieldObject().inModalMode()) {
      return false;
    }

    // Firefox will allow the first node in an iframe to be deleted
    // on a backspace.  Disallow it if the node is empty.
    if (e.keyCode == goog.events.KeyCodes.BACKSPACE) {
      this.handleBackspaceInternal(e, this.getFieldObject().getRange());
    } else if (e.keyCode == goog.events.KeyCodes.DELETE) {
      this.handleDeleteGecko(e);
    }
  }

  return false;
};


/** @override */
goog.editor.plugins.EnterHandler.prototype.handleKeyPress = function(e) {
  'use strict';
  // ENTER must be handled in keyPress as it requires a beforechange event,
  // which is fired in between keydown and keyup.
  if (e.keyCode == goog.events.KeyCodes.ENTER) {
    if (goog.userAgent.GECKO) {
      if (!e.shiftKey) {
        // Behave similarly to IE's content editable return carriage:
        // If the shift key is down or specified by the application, insert a
        // BR, otherwise split paragraphs
        this.handleEnterGecko_(e);
      }
    } else {
      // In Gecko-based browsers, this is handled in the handleEnterGecko_
      // method.
      this.getFieldObject().dispatchBeforeChange();
      var cursorPosition = this.deleteCursorSelection_();

      var split = !!this.getFieldObject().execCommand(
          goog.editor.plugins.Blockquote.SPLIT_COMMAND, cursorPosition);
      if (split) {
        // TODO(user): I think we probably don't need to stopPropagation here
        e.preventDefault();
        e.stopPropagation();
      }

      this.releasePositionObject_(cursorPosition);

      if (goog.userAgent.WEBKIT) {
        this.handleEnterWebkitInternal(e);
      }

      this.processParagraphTagsInternal(e, split);
      this.getFieldObject().dispatchChange();
    }
  }

  return false;
};


/** @override */
goog.editor.plugins.EnterHandler.prototype.handleKeyUp = function(e) {
  'use strict';
  // If a dialog doesn't have selectable field, Gecko grabs the event and
  // performs actions in editor window. This solves that problem and allows
  // the event to be passed on to proper handlers.
  if (goog.userAgent.GECKO && this.getFieldObject().inModalMode()) {
    return false;
  }
  this.handleKeyUpInternal(e);
  return false;
};


/**
 * Internal handler for keyup events.
 * @param {goog.events.Event} e The key event.
 * @protected
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.editor.plugins.EnterHandler.prototype.handleKeyUpInternal = function(e) {
  'use strict';
  if ((goog.userAgent.IE) && e.keyCode == goog.events.KeyCodes.ENTER) {
    this.ensureBlockIeOpera(goog.dom.TagName.DIV, true);
  }
};


/**
 * Handles an enter keypress event on fields in Gecko.
 * @param {goog.events.BrowserEvent} e The key event.
 * @private
 */
goog.editor.plugins.EnterHandler.prototype.handleEnterGecko_ = function(e) {
  'use strict';
  // Retrieve whether the selection is collapsed before we delete it.
  var range = this.getFieldObject().getRange();
  var wasCollapsed = !range || range.isCollapsed();
  var cursorPosition = this.deleteCursorSelection_();

  var handled = this.getFieldObject().execCommand(
      goog.editor.plugins.Blockquote.SPLIT_COMMAND, cursorPosition);
  if (handled) {
    // TODO(user): I think we probably don't need to stopPropagation here
    e.preventDefault();
    e.stopPropagation();
  }

  this.releasePositionObject_(cursorPosition);
  if (!handled) {
    this.handleEnterAtCursorGeckoInternal(e, wasCollapsed, range);
  }
};


/**
 * Handle an enter key press in WebKit.
 * @param {goog.events.BrowserEvent} e The key press event.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.handleEnterWebkitInternal = function(
    e) {};


/**
 * Handle an enter key press on collapsed selection.  handleEnterGecko_ ensures
 * the selection is collapsed by deleting its contents if it is not.  The
 * default implementation does nothing.
 * @param {goog.events.BrowserEvent} e The key press event.
 * @param {boolean} wasCollapsed Whether the selection was collapsed before
 *     the key press.  If it was not, code before this function has already
 *     cleared the contents of the selection.
 * @param {goog.dom.AbstractRange} range Object representing the selection.
 * @protected
 */
goog.editor.plugins.EnterHandler.prototype.handleEnterAtCursorGeckoInternal =
    function(e, wasCollapsed, range) {};


/**
 * Names of all the nodes that we don't want to turn into block nodes in IE when
 * the user hits enter.
 * @type {Object}
 * @private
 */
goog.editor.plugins.EnterHandler.DO_NOT_ENSURE_BLOCK_NODES_ =
    goog.object.createSet(
        goog.dom.TagName.LI, goog.dom.TagName.DIV, goog.dom.TagName.H1,
        goog.dom.TagName.H2, goog.dom.TagName.H3, goog.dom.TagName.H4,
        goog.dom.TagName.H5, goog.dom.TagName.H6);


/**
 * Whether this is a node that contains a single BR tag and non-nbsp
 * whitespace.
 * @param {Node} node Node to check.
 * @return {boolean} Whether this is an element that only contains a BR.
 * @protected
 */
goog.editor.plugins.EnterHandler.isBrElem = function(node) {
  'use strict';
  return goog.editor.node.isEmpty(node) &&
      goog.dom
          .getElementsByTagName(
              goog.dom.TagName.BR, /** @type {!Element} */ (node))
          .length == 1;
};


/**
 * Ensures all text in IE and Opera to be in the given tag in order to control
 * Enter spacing. Call this when Enter is pressed if desired.
 *
 * We want to make sure the user is always inside of a block (or other nodes
 * listed in goog.editor.plugins.EnterHandler.IGNORE_ENSURE_BLOCK_NODES_).  We
 * listen to keypress to force nodes that the user is leaving to turn into
 * blocks, but we also need to listen to keyup to force nodes that the user is
 * entering to turn into blocks.
 * Example:  html is: `<h2>foo[cursor]</h2>`, and the user hits enter.  We
 * don't want to format the h2, but we do want to format the P that is
 * created on enter.  The P node is not available until keyup.
 * @param {!goog.dom.TagName} tag The tag name to convert to.
 * @param {boolean=} opt_keyUp Whether the function is being called on key up.
 *     When called on key up, the cursor is in the newly created node, so the
 *     semantics for when to change it to a block are different.  Specifically,
 *     if the resulting node contains only a BR, it is converted to `<tag>`.
 * @protected
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.editor.plugins.EnterHandler.prototype.ensureBlockIeOpera = function(
    tag, opt_keyUp) {
  'use strict';
  var range = this.getFieldObject().getRange();
  var container = range.getContainer();
  var field = this.getFieldObject().getElement();

  while (container && container != field) {
    // We don't need to ensure a block if we are already in the same block, or
    // in another block level node that we don't want to change the format of
    // (unless we're handling keyUp and that block node just contains a BR).
    var nodeName = container.nodeName;
    // Due to @bug 2455389, the call to isBrElem needs to be inlined in the if
    // instead of done before and saved in a variable, so that it can be
    // short-circuited and avoid a weird IE edge case.
    if (nodeName == tag ||
        (goog.editor.plugins.EnterHandler
             .DO_NOT_ENSURE_BLOCK_NODES_[nodeName] &&
         !(opt_keyUp &&
           goog.editor.plugins.EnterHandler.isBrElem(container)))) {

      return;
    }

    container = container.parentNode;
  }

  this.getFieldObject().getEditableDomHelper().getDocument().execCommand(
      'FormatBlock', false, '<' + tag + '>');
};


/**
 * Deletes the content at the current cursor position.
 * @return {!Node|!Object} Something representing the current cursor position.
 *    See deleteCursorSelectionIE_ and deleteCursorSelectionW3C_ for details.
 *    Should be passed to releasePositionObject_ when no longer in use.
 * @private
 */
goog.editor.plugins.EnterHandler.prototype.deleteCursorSelection_ = function() {
  'use strict';
  return goog.editor.BrowserFeature.HAS_W3C_RANGES ?
      this.deleteCursorSelectionW3C_() :
      this.deleteCursorSelectionIE_();
};


/**
 * Releases the object returned by deleteCursorSelection_.
 * @param {Node|Object} position The object returned by deleteCursorSelection_.
 * @private
 */
goog.editor.plugins.EnterHandler.prototype.releasePositionObject_ = function(
    position) {
  'use strict';
  if (!goog.editor.BrowserFeature.HAS_W3C_RANGES) {
    goog.dom.removeNode(/** @type {!Node} */ (position));
  }
};


/**
 * Delete the selection at the current cursor position, then returns a temporary
 * node at the current position.
 * @return {!Node} A temporary node marking the current cursor position. This
 *     node should eventually be removed from the DOM.
 * @private
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.editor.plugins.EnterHandler.prototype.deleteCursorSelectionIE_ =
    function() {
  'use strict';
  var doc = this.getFieldDomHelper().getDocument();
  var range = doc.selection.createRange();

  var id = goog.string.createUniqueString();
  range.pasteHTML('<span id="' + id + '"></span>');
  var splitNode = doc.getElementById(id);
  splitNode.id = '';
  return splitNode;
};


/**
 * Delete the selection at the current cursor position, then returns the node
 * at the current position.
 * @return {!goog.editor.range.Point} The current cursor position. Note that
 *    unlike simulateEnterIE_, this should not be removed from the DOM.
 * @private
 */
goog.editor.plugins.EnterHandler.prototype.deleteCursorSelectionW3C_ =
    function() {
  'use strict';
  var range = this.getFieldObject().getRange();

  // Delete the current selection if it's is non-collapsed.
  // Although this is redundant in FF, it's necessary for Safari
  if (range && !range.isCollapsed()) {
    var shouldDelete = true;
    // Opera selects the <br> in an empty block if there is no text node
    // preceding it. To preserve inline formatting when pressing [enter] inside
    // an empty block, don't delete the selection if it only selects a <br> at
    // the end of the block.
    // TODO(user): Move this into goog.dom.Range. It should detect this state
    // when creating a range from the window selection and fix it in the created
    // range.
    if (shouldDelete) {
      goog.editor.plugins.EnterHandler.deleteW3cRange_(range);
    }
  }

  return goog.editor.range.getDeepEndPoint(range, true);
};

/**
 * Checks Whether the selection range start from leftmost.
 * @param {?Node} node the element or text node the range starts in.
 * @param {?Node} baseNode the container element.
 * @return {boolean} Whether the selection range start from leftmost.
 * @private
 */
goog.editor.plugins.EnterHandler.isNodeLeftMostChild_ = function(
    node, baseNode) {
  'use strict';
  let childNode = node;
  while (childNode && childNode.nodeName != goog.dom.TagName.BODY &&
         childNode != baseNode) {
    if (childNode.previousSibling) {
      return false;
    }
    childNode = childNode.parentNode;
  }
  return true;
};

/**
 * Deletes the contents of the selection from the DOM.
 * @param {goog.dom.AbstractRange} range The range to remove contents from.
 * @return {goog.dom.AbstractRange} The resulting range. Used for testing.
 * @private
 */
goog.editor.plugins.EnterHandler.deleteW3cRange_ = function(range) {
  'use strict';
  if (range && !range.isCollapsed()) {
    var reselect = true;
    var baseNode = range.getContainerElement();
    var nodeOffset = new goog.dom.NodeOffset(range.getStartNode(), baseNode);
    var rangeOffset = range.getStartOffset();

    // Whether the selection crosses no container boundaries.
    var isInOneContainer =
        goog.editor.plugins.EnterHandler.isInOneContainerW3c_(range);

    // Whether the selection starts in a container.
    var isPartialStart = !isInOneContainer && range.getStartOffset() != 0;
    // Whether the selection ends in a container it doesn't fully select.
    var isPartialEnd = !isInOneContainer &&
        goog.editor.plugins.EnterHandler.isPartialEndW3c_(range);

    var isNodeLeftMostChild =
        goog.editor.plugins.EnterHandler.isNodeLeftMostChild_(
            range.getStartNode(), baseNode);

    // Remove The range contents, and ensure the correct content stays selected.
    range.removeContents();
    var node = nodeOffset.findTargetNode(baseNode);
    if (node) {
      range = goog.dom.Range.createCaret(node, rangeOffset);
    } else {
      // when the node that would have been referenced has now been deleted and
      // there are no other nodes in the baseNode,  Thus need to set the caret
      // to the end of the base node. If selection range start from leftmost,
      // set the caret to the start of the base node.
      var pos = isNodeLeftMostChild ? 0 : baseNode.childNodes.length;
      range = goog.dom.Range.createCaret(baseNode, pos);
      reselect = false;
    }
    range.select();

    // If we just deleted everything from the container, add an nbsp
    // to the container, and leave the cursor inside of it
    if (isInOneContainer) {
      var container = goog.editor.style.getContainer(range.getStartNode());
      if (goog.editor.node.isEmpty(container, true)) {
        var html = '&nbsp;';
        goog.editor.node.replaceInnerHtml(container, html);
        goog.editor.range.selectNodeStart(container.firstChild);
        reselect = false;
      }
    }

    if (isPartialStart && isPartialEnd) {
      /*
       This code handles the following, where | is the cursor:
         <div>a|b</div><div>c|d</div>
       After removeContents, the remaining HTML is
         <div>a</div><div>d</div>
       which means the line break between the two divs remains.  This block
       moves children of the second div in to the first div to get the correct
       result:
         <div>ad</div>

       TODO(robbyw): Should we wrap the second div's contents in a span if they
                     have inline style?
      */
      var rangeStart = goog.editor.style.getContainer(range.getStartNode());
      var redundantContainer = goog.editor.node.getNextSibling(rangeStart);
      if (rangeStart && redundantContainer) {
        goog.dom.append(rangeStart, redundantContainer.childNodes);
        goog.dom.removeNode(redundantContainer);
      }
    }

    if (reselect) {
      // The contents of the original range are gone, so restore the cursor
      // position at the start of where the range once was.
      range = goog.dom.Range.createCaret(
          nodeOffset.findTargetNode(baseNode), rangeOffset);
      range.select();
    }
  }

  return range;
};


/**
 * Checks whether the whole range is in a single block-level element.
 * @param {goog.dom.AbstractRange} range The range to check.
 * @return {boolean} Whether the whole range is in a single block-level element.
 * @private
 */
goog.editor.plugins.EnterHandler.isInOneContainerW3c_ = function(range) {
  'use strict';
  // Find the block element containing the start of the selection.
  var startContainer = range.getStartNode();
  if (goog.editor.style.isContainer(startContainer)) {
    startContainer =
        startContainer.childNodes[range.getStartOffset()] || startContainer;
  }
  startContainer = goog.editor.style.getContainer(startContainer);

  // Find the block element containing the end of the selection.
  var endContainer = range.getEndNode();
  if (goog.editor.style.isContainer(endContainer)) {
    endContainer =
        endContainer.childNodes[range.getEndOffset()] || endContainer;
  }
  endContainer = goog.editor.style.getContainer(endContainer);

  // Compare the two.
  return startContainer == endContainer;
};


/**
 * Checks whether the end of the range is not at the end of a block-level
 * element.
 * @param {goog.dom.AbstractRange} range The range to check.
 * @return {boolean} Whether the end of the range is not at the end of a
 *     block-level element.
 * @private
 */
goog.editor.plugins.EnterHandler.isPartialEndW3c_ = function(range) {
  'use strict';
  var endContainer = range.getEndNode();
  var endOffset = range.getEndOffset();
  // Since the range object is different for each browser,
  // Normalize range using previousSibling or parentNode of
  // endContainer, when endOffset is 0.
  while (endOffset === 0 && endContainer) {
    if (endContainer.previousSibling) {
      endContainer = endContainer.previousSibling;
      endOffset = goog.editor.node.getLength(endContainer);
    } else if (endContainer.parentNode) {
      endContainer = endContainer.parentNode;
      endOffset = 0;
    } else {
      break;
    }
  }

  var node = endContainer;
  if (goog.editor.style.isContainer(node)) {
    var child = node.childNodes[endOffset];
    // Child is null when end offset is >= length, which indicates the entire
    // container is selected.  Otherwise, we also know the entire container
    // is selected if the selection ends at a new container.
    if (!child ||
        child.nodeType == goog.dom.NodeType.ELEMENT &&
            goog.editor.style.isContainer(child)) {
      return false;
    }
  }

  var container = goog.editor.style.getContainer(node);
  while (container != node) {
    if (goog.editor.node.getNextSibling(node)) {
      return true;
    }
    node = node.parentNode;
  }

  return endOffset != goog.editor.node.getLength(endContainer);
};
