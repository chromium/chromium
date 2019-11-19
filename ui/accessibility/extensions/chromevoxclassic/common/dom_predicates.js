// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A bunch of predicates that take as input an array of
 * nodes with the unique ancestors of a node. They output true if a
 * certain category of node has been found.
 *
 */

goog.provide('cvox.DomPredicates');


/**
 * Checkbox.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a checkbox.
 */
cvox.DomPredicates.checkboxPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute &&
         nodes[i].getAttribute('role') == 'checkbox') ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'checkbox')) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Radio button.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a radio button.
 */
cvox.DomPredicates.radioPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'radio') ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'radio')) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Slider.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a slider.
 */
cvox.DomPredicates.sliderPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'slider') ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'range')) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Graphic.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a graphic.
 */
cvox.DomPredicates.graphicPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i].tagName == 'IMG' ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'img')) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Button.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a button.
 */
cvox.DomPredicates.buttonPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'button') ||
        nodes[i].tagName == 'BUTTON' ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'submit') ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'button') ||
        (nodes[i].tagName == 'INPUT' && nodes[i].type == 'reset')) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Combo box.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a combo box.
 */
cvox.DomPredicates.comboBoxPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute &&
         nodes[i].getAttribute('role') == 'combobox') ||
        nodes[i].tagName == 'SELECT') {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Editable text field.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is an editable text field.
 */
cvox.DomPredicates.editTextPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'textbox') ||
        nodes[i].tagName == 'TEXTAREA' ||
        nodes[i].isContentEditable ||
        (nodes[i].tagName == 'INPUT' &&
        cvox.DomUtil.isInputTypeText(nodes[i]))) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Heading.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading.
 */
cvox.DomPredicates.headingPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i].getAttribute &&
        nodes[i].getAttribute('role') == 'heading') {
      return nodes[i];
    }
    switch (nodes[i].tagName) {
      case 'H1':
      case 'H2':
      case 'H3':
      case 'H4':
      case 'H5':
      case 'H6':
        return nodes[i];
    }
  }
  return null;
};


/**
 * Heading level 1.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 1.
 * TODO: handle ARIA headings with ARIA heading levels?
 */
cvox.DomPredicates.heading1Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H1');
};


/**
 * Heading level 2.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 2.
 */
cvox.DomPredicates.heading2Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H2');
};


/**
 * Heading level 3.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 3.
 */
cvox.DomPredicates.heading3Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H3');
};


/**
 * Heading level 4.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 4.
 */
cvox.DomPredicates.heading4Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H4');
};


/**
 * Heading level 5.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 5.
 */
cvox.DomPredicates.heading5Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H5');
};


/**
 * Heading level 6.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a heading level 6.
 */
cvox.DomPredicates.heading6Predicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'H6');
};


/**
 * Link.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a link.
 */
cvox.DomPredicates.linkPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'link') ||
        (nodes[i].tagName == 'A' && nodes[i].href)) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Table.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a data table.
 */
cvox.DomPredicates.tablePredicate = function(nodes) {
  // TODO(stoarca): Captions should always be allowed!!
  var node = cvox.DomUtil.findTableNodeInList(nodes, {allowCaptions: true});
  if (node && !cvox.DomUtil.isLayoutTable(node)) {
    return node;
  } else {
    return null;
  }
};

/**
 * Table Cell.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a table cell.
 */
cvox.DomPredicates.cellPredicate = function(nodes) {
  for (var i = nodes.length - 1; i >= 0; --i) {
    var node = nodes[i];
    if (node.tagName == 'TD' ||
        node.tagName == 'TH' ||
        (node.getAttribute && node.getAttribute('role') == 'gridcell')) {
      return node;
    }
  }
  return null;
};


/**
 * Visited link.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a visited link.
 */
cvox.DomPredicates.visitedLinkPredicate = function(nodes) {
  for (var i = nodes.length - 1; i >= 0; --i) {
    if (cvox.DomPredicates.linkPredicate([nodes[i]]) &&
        cvox.ChromeVox.visitedUrls[nodes[i].href]) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * List.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a list.
 */
cvox.DomPredicates.listPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'list') ||
        nodes[i].tagName == 'UL' ||
        nodes[i].tagName == 'OL') {
      return nodes[i];
    }
  }
  return null;
};


/**
 * List item.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a list item.
 */
cvox.DomPredicates.listItemPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute &&
         nodes[i].getAttribute('role') == 'listitem') ||
        nodes[i].tagName == 'LI') {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Blockquote.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a blockquote.
 */
cvox.DomPredicates.blockquotePredicate = function(nodes) {
  return cvox.DomPredicates.containsTagName_(nodes, 'BLOCKQUOTE');
};


/**
 * Form field.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is any type of form field.
 */
cvox.DomPredicates.formFieldPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (cvox.DomUtil.isControl(nodes[i])) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * ARIA landmark.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is an ARIA landmark.
 */
cvox.DomPredicates.landmarkPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (cvox.AriaUtil.isLandmark(nodes[i])) {
      return nodes[i];
    }
  }
  return null;
};


/**
 * @param {Array} arr Array of nodes.
 * @param {string} tagName The name of the tag.
 * @return {?Node} Node if obj is in the array.
 * @private
 */
cvox.DomPredicates.containsTagName_ = function(arr, tagName) {
  var i = arr.length;
  while (i--) {
    if (arr[i].tagName == tagName) {
      return arr[i];
    }
  }
  return null;
};


/**
 * MathML expression
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a math expression.
 */
cvox.DomPredicates.mathPredicate = function(nodes) {
  return cvox.DomUtil.findMathNodeInList(nodes);
};

/**
 * SECTION: A section is anything that indicates a new section. This includes
 * headings and landmarks.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is considered a section marker.
 */
cvox.DomPredicates.sectionPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (cvox.DomUtil.isSemanticElt(nodes[i])) {
      return nodes[i];
    }
    if (cvox.AriaUtil.isLandmark(nodes[i])) {
      return nodes[i];
    }
    if (nodes[i].getAttribute &&
        nodes[i].getAttribute('role') == 'heading') {
      return nodes[i];
    }
    switch (nodes[i].tagName) {
      case 'H1':
      case 'H2':
      case 'H3':
      case 'H4':
      case 'H5':
      case 'H6':
        return nodes[i];
    }
  }
  return null;
};

/**
 * CONTROL: A control is anything that the user can interact with. This includes
 * form fields and links.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is considered a control.
 */
cvox.DomPredicates.controlPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (cvox.DomUtil.isControl(nodes[i])) {
      return nodes[i];
    }
    if ((nodes[i].getAttribute && nodes[i].getAttribute('role') == 'link') ||
        (nodes[i].tagName == 'A' && nodes[i].href)) {
      return nodes[i];
    }
  }
  return null;
};

/**
 * Caption.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a caption.
 */
cvox.DomPredicates.captionPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i].tagName == 'CAPTION') {
      return nodes[i];
    }
  }
  return null;
};

/**
 * Article.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a article.
 */
cvox.DomPredicates.articlePredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if ((nodes[i].getAttribute &&
            nodes[i].getAttribute('role') == 'article') ||
        nodes[i].tagName == 'ARTICLE') {
      return nodes[i];
    }
  }
  return null;
};

/**
 * Media.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a media widget (video or audio).
 */
cvox.DomPredicates.mediaPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i].tagName == 'AUDIO' ||
        nodes[i].tagName == 'VIDEO') {
      return nodes[i];
    }
  }
  return null;
};


/**
 * Ordered List.
 * @param {Array<Node>} nodes An array of nodes to check.
 * @return {?Node} Node in the array that is a ordered list.
 */
cvox.DomPredicates.orderedListPredicate = function(nodes) {
  for (var i = 0; i < nodes.length; i++) {
    if (nodes[i].tagName == 'OL') {
      return nodes[i];
    }
  }
  return null;
};
