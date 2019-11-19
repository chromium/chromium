// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A utility class for building NavDescriptions from the dom.
 */


goog.provide('cvox.DescriptionUtil');

goog.require('cvox.AriaUtil');
goog.require('cvox.AuralStyleUtil');
goog.require('cvox.BareObjectWalker');
goog.require('cvox.CursorSelection');
goog.require('cvox.DomUtil');
goog.require('cvox.EarconUtil');
goog.require('cvox.MathmlStore');
goog.require('cvox.NavDescription');
goog.require('cvox.SpeechRuleEngine');
goog.require('cvox.TraverseMath');


/**
 * Lists all Node tagName's who's description is derived from its subtree.
 * @type {Object<boolean>}
 */
cvox.DescriptionUtil.COLLECTION_NODE_TYPE = {
  'H1': true,
  'H2': true,
  'H3': true,
  'H4': true,
  'H5': true,
  'H6': true
};

/**
 * Get a control's complete description in the same format as if you
 *     navigated to the node.
 * @param {Element} control A control.
 * @param {Array<Node>=} opt_changedAncestors The changed ancestors that will
 * be used to determine what needs to be spoken. If this is not provided, the
 * ancestors used to determine what needs to be spoken will just be the control
 * itself and its surrounding control if it has one.
 * @return {cvox.NavDescription} The description of the control.
 */
cvox.DescriptionUtil.getControlDescription =
    function(control, opt_changedAncestors) {
  var ancestors = [control];
  if (opt_changedAncestors && (opt_changedAncestors.length > 0)) {
    ancestors = opt_changedAncestors;
  } else {
    var surroundingControl = cvox.DomUtil.getSurroundingControl(control);
    if (surroundingControl) {
      ancestors = [surroundingControl, control];
    }
  }

  var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
      ancestors, true, cvox.VERBOSITY_VERBOSE);

  // Use heuristics if the control doesn't otherwise have a name.
  if (surroundingControl) {
    var name = cvox.DomUtil.getName(surroundingControl);
    if (name.length == 0) {
      name = cvox.DomUtil.getControlLabelHeuristics(surroundingControl);
      if (name.length > 0) {
        description.context = name + ' ' + description.context;
      }
    }
  } else {
    var name = cvox.DomUtil.getName(control);
    if (name.length == 0) {
      name = cvox.DomUtil.getControlLabelHeuristics(control);
      if (name.length > 0) {
        description.text = cvox.DomUtil.collapseWhitespace(name);
      }
    }
    var value = cvox.DomUtil.getValue(control);
    if (value.length > 0) {
      description.userValue = cvox.DomUtil.collapseWhitespace(value);
    }
  }

  return description;
};


/**
 * Returns a description of a navigation from an array of changed
 * ancestor nodes. The ancestors are in order from the highest in the
 * tree to the lowest, i.e. ending with the current leaf node.
 *
 * @param {Array<Node>} ancestorsArray An array of ancestor nodes.
 * @param {boolean} recursive Whether or not the element's subtree should
 *     be used; true by default.
 * @param {number} verbosity The verbosity setting.
 * @return {cvox.NavDescription} The description of the navigation action.
 */
cvox.DescriptionUtil.getDescriptionFromAncestors = function(
    ancestorsArray, recursive, verbosity) {
  if (typeof(recursive) === 'undefined') {
    recursive = true;
  }
  var len = ancestorsArray.length;
  var context = '';
  var text = '';
  var userValue = '';
  var annotation = '';
  var earcons = [];
  var personality = null;
  var hint = '';

  if (len > 0) {
    text = cvox.DomUtil.getName(ancestorsArray[len - 1], recursive);

    userValue = cvox.DomUtil.getValue(ancestorsArray[len - 1]);
  }
  for (var i = len - 1; i >= 0; i--) {
    var node = ancestorsArray[i];

    hint = cvox.DomUtil.getHint(node);

    // Don't speak dialogs here, they're spoken when events occur.
    var role = node.getAttribute ? node.getAttribute('role') : null;
    if (role == 'alertdialog') {
      continue;
    }

    var roleText = cvox.DomUtil.getRole(node, verbosity);

    // Use the ancestor closest to the target to be the personality.
    if (!personality) {
      personality = cvox.AuralStyleUtil.getStyleForNode(node);
    }
    // TODO(dtseng): Is this needed?
    if (i < len - 1 && node.hasAttribute('role')) {
      var name = cvox.DomUtil.getName(node, false);
      if (name) {
        roleText = name + ' ' + roleText;
      }
    }
    if (roleText.length > 0) {
      // Since we prioritize reading of context in reading order, only populate
      // it for larger ancestry changes.
      if (context.length > 0 ||
          (annotation.length > 0 && node.childElementCount > 1)) {
        context = roleText + ' ' + cvox.DomUtil.getState(node, false) +
                  ' ' + context;
      } else {
        if (annotation.length > 0) {
          annotation +=
              ' ' + roleText + ' ' + cvox.DomUtil.getState(node, true);
        } else {
          annotation = roleText + ' ' + cvox.DomUtil.getState(node, true);
        }
      }
    }
    var earcon = cvox.EarconUtil.getEarcon(node);
    if (earcon != null && earcons.indexOf(earcon) == -1) {
      earcons.push(earcon);
    }
  }
  return new cvox.NavDescription({
    context: cvox.DomUtil.collapseWhitespace(context),
    text: cvox.DomUtil.collapseWhitespace(text),
    userValue: cvox.DomUtil.collapseWhitespace(userValue),
    annotation: cvox.DomUtil.collapseWhitespace(annotation),
    earcons: earcons,
    personality: personality,
    hint: cvox.DomUtil.collapseWhitespace(hint)
  });
};

/**
 * Returns a description of a navigation from an array of changed
 * ancestor nodes. The ancestors are in order from the highest in the
 * tree to the lowest, i.e. ending with the current leaf node.
 *
 * @param {Node} prevNode The previous node in navigation.
 * @param {Node} node The current node in navigation.
 * @param {boolean} recursive Whether or not the element's subtree should
 *     be used; true by default.
 * @param {number} verbosity The verbosity setting.
 * @return {!Array<cvox.NavDescription>} The description of the navigation
 * action.
 */
cvox.DescriptionUtil.getDescriptionFromNavigation =
    function(prevNode, node, recursive, verbosity) {
  if (!prevNode || !node) {
    return [];
  }

  // Specialized math descriptions.
  if (cvox.DomUtil.isMath(node) &&
      !cvox.AriaUtil.isMath(node)) {
    return cvox.DescriptionUtil.getMathDescription(node);
  }

  // Next, check to see if the current node is a collection type.
  if (cvox.DescriptionUtil.COLLECTION_NODE_TYPE[node.tagName]) {
    return cvox.DescriptionUtil.getCollectionDescription(
        /** @type {!cvox.CursorSelection} */(
            cvox.CursorSelection.fromNode(prevNode)),
        /** @type {!cvox.CursorSelection} */(
            cvox.CursorSelection.fromNode(node)));
  }

  // Now, generate a description for all other elements.
  var ancestors = cvox.DomUtil.getUniqueAncestors(prevNode, node, true);
  var desc = cvox.DescriptionUtil.getDescriptionFromAncestors(
      ancestors, recursive, verbosity);
  var prevAncestors = cvox.DomUtil.getUniqueAncestors(node, prevNode);
  if (cvox.DescriptionUtil.shouldDescribeExit_(prevAncestors)) {
    var prevDesc = cvox.DescriptionUtil.getDescriptionFromAncestors(
        prevAncestors, recursive, verbosity);
    if (prevDesc.context && !desc.context) {
      desc.context =
          Msgs.getMsg('exited_container', [prevDesc.context]);
    }
  }
  return [desc];
};


/**
 * Returns an array of NavDescriptions that includes everything that would be
 * spoken by an object walker while traversing from prevSel to sel.
 * It also includes any necessary annotations and context about the set of
 * descriptions. This function is here because most (currently all) walkers
 * that iterate over non-leaf nodes need this sort of description.
 * This is an awkward design, and should be changed in the future.
 * @param {!cvox.CursorSelection} prevSel The previous selection.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {!Array<!cvox.NavDescription>} The descriptions as described above.
 */
cvox.DescriptionUtil.getCollectionDescription = function(prevSel, sel) {
  var descriptions = cvox.DescriptionUtil.getRawDescriptions_(prevSel, sel);
  cvox.DescriptionUtil.insertCollectionDescription_(descriptions);
  return descriptions;
};


/**
 * Used for getting collection descriptions.
 * @type {!cvox.BareObjectWalker}
 * @private
 */
cvox.DescriptionUtil.subWalker_ = new cvox.BareObjectWalker();


/**
 * Returns the descriptions that would be gotten by an object walker.
 * @param {!cvox.CursorSelection} prevSel The previous selection.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {!Array<!cvox.NavDescription>} The descriptions.
 * @private
 */
cvox.DescriptionUtil.getRawDescriptions_ = function(prevSel, sel) {
  // Use a object walker in non-smart mode to traverse all of the
  // nodes inside the current smart node and return their annotations.
  var descriptions = [];

  // We want the descriptions to be in forward order whether or not the
  // selection is reversed.
  sel = sel.clone().setReversed(false);
  var node = cvox.DescriptionUtil.subWalker_.sync(sel).start.node;

  var prevNode = prevSel.end.node;
  var curSel = cvox.CursorSelection.fromNode(node);

  if (!curSel) {
    return [];
  }

  while (cvox.DomUtil.isDescendantOfNode(node, sel.start.node)) {
    var ancestors = cvox.DomUtil.getUniqueAncestors(prevNode, node);
    // Specialized math descriptions.
    if (cvox.DomUtil.isMath(node) &&
        !cvox.AriaUtil.isMath(node)) {
      descriptions =
          descriptions.concat(cvox.DescriptionUtil.getMathDescription(node));
    } else {
      var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
          ancestors, true, cvox.ChromeVox.verbosity);
      descriptions.push(description);
    }
    curSel = cvox.DescriptionUtil.subWalker_.next(curSel);
    if (!curSel) {
      break;
    }

    curSel = /** @type {!cvox.CursorSelection} */ (curSel);
    prevNode = node;
    node = curSel.start.node;
  }

  return descriptions;
};

/**
 * Returns the full descriptions of the child nodes that would be gotten by an
 * object walker.
 * @param {?Node} prevnode The previous element if there is one.
 * @param {!Node} node The target element.
 * @return {!Array<!cvox.NavDescription>} The descriptions.
 */
cvox.DescriptionUtil.getFullDescriptionsFromChildren =
    function(prevnode, node) {
  var descriptions = [];
  if (!node) {
    return descriptions;
  }
  var desc;
  if (cvox.DomUtil.isLeafNode(node)) {
    var ancestors;
    if (prevnode) {
      ancestors = cvox.DomUtil.getUniqueAncestors(prevnode, node);
    } else {
      ancestors = new Array();
      ancestors.push(node);
    }
    desc = cvox.DescriptionUtil.getDescriptionFromAncestors(
        ancestors, true, cvox.ChromeVox.verbosity);
    descriptions.push(desc);
    return descriptions;
  }
  var originalNode = node;
  var curSel = cvox.CursorSelection.fromNode(node);
  if (!curSel) {
    return descriptions;
  }
  var newNode = cvox.DescriptionUtil.subWalker_.sync(curSel).start.node;
  curSel = cvox.CursorSelection.fromNode(newNode);
  if (!curSel) {
    return descriptions;
  }
  while (newNode && cvox.DomUtil.isDescendantOfNode(newNode, originalNode)) {
    descriptions = descriptions.concat(
        cvox.DescriptionUtil.getFullDescriptionsFromChildren(
            prevnode, newNode));
    curSel = cvox.DescriptionUtil.subWalker_.next(curSel);
    if (!curSel) {
      break;
    }
    curSel = /** @type {!cvox.CursorSelection} */ (curSel);
    prevnode = newNode;
    newNode = curSel.start.node;
  }
  return descriptions;
};


/**
 * Modify the descriptions to say that it is a collection.
 * @param {Array<cvox.NavDescription>} descriptions The descriptions.
 * @private
 */
cvox.DescriptionUtil.insertCollectionDescription_ = function(descriptions) {
  var annotations = cvox.DescriptionUtil.getAnnotations_(descriptions);
  // If all of the items have the same annotation, describe it as a
  // <annotation> collection with <n> items. Currently only enabled
  // for links, but support should be added for any other type that
  // makes sense.
  if (descriptions.length >= 3 &&
      descriptions[0].context.length == 0 &&
      annotations.length == 1 &&
      annotations[0].length > 0 &&
      cvox.DescriptionUtil.isAnnotationCollection_(annotations[0])) {
    var commonAnnotation = annotations[0];
    var firstContext = descriptions[0].context;
    descriptions[0].context = '';
    for (var i = 0; i < descriptions.length; i++) {
      descriptions[i].annotation = '';
    }

    descriptions.splice(0, 0, new cvox.NavDescription({
      context: firstContext,
      text: '',
      annotation: Msgs.getMsg(
          'collection',
          [commonAnnotation,
           Msgs.getNumber(descriptions.length)])
    }));
  }
};


/**
 * Pulls the annotations from a description array.
 * @param {Array<cvox.NavDescription>} descriptions The descriptions.
 * @return {Array<string>} The annotations.
 * @private
 */
cvox.DescriptionUtil.getAnnotations_ = function(descriptions) {
  var annotations = [];
  for (var i = 0; i < descriptions.length; ++i) {
    var description = descriptions[i];
    if (annotations.indexOf(description.annotation) == -1) {
      // If we have an Internal link collection, call it Link collection.
      // NOTE(deboer): The message comparison is a symptom of a bad design.
      // I suspect this code belongs elsewhere but I don't know where, yet.
      var linkMsg = Msgs.getMsg('role_link');
      if (description.annotation.toLowerCase().indexOf(linkMsg.toLowerCase()) !=
          -1) {
        if (annotations.indexOf(linkMsg) == -1) {
          annotations.push(linkMsg);
        }
      } else {
        annotations.push(description.annotation);
      }
    }
  }
  return annotations;
};


/**
 * Returns true if this annotation should be grouped as a collection,
 * meaning that instead of repeating the annotation for each item, we
 * just announce <annotation> collection with <n> items at the front.
 *
 * Currently enabled for links, but could be extended to support other
 * roles that make sense.
 *
 * @param {string} annotation The annotation text.
 * @return {boolean} If this annotation should be a collection.
 * @private
 */
cvox.DescriptionUtil.isAnnotationCollection_ = function(annotation) {
  return (annotation == Msgs.getMsg('role_link'));
};

/**
 * Determines whether to describe the exit of an ancestor chain.
 * @param {Array<Node>} ancestors The ancestors exited during navigation.
 * @return {boolean} The result.
 * @private
 */
cvox.DescriptionUtil.shouldDescribeExit_ = function(ancestors) {
  return ancestors.some(function(node) {
    switch (node.tagName) {
      case 'TABLE':
      case 'MATH':
        return true;
    }
    return cvox.AriaUtil.isLandmark(node);
  });
};


// TODO(sorge): Bad naming...this thing returns *multiple* descriptions.
/**
 * Generates a description for a math node.
 * @param {Node} node The given node.
 * @return {!Array<cvox.NavDescription>} A list of Navigation descriptions.
 */
cvox.DescriptionUtil.getMathDescription = function(node) {
  if (!node) {
    return [];
  }
  // TODO (sorge) This function should evantually be removed. Descriptions
  //     should come directly from the speech rule engine, taking information on
  //     verbosity etc. into account.
  var speechEngine = cvox.SpeechRuleEngine.getInstance();
  var traverse = cvox.TraverseMath.getInstance();
  speechEngine.parameterize(cvox.MathmlStore.getInstance());
  traverse.initialize(node);
  var ret = speechEngine.evaluateNode(traverse.activeNode);
  if (ret == []) {
    return [new cvox.NavDescription({'text': 'empty math'})];
  }
  if (cvox.ChromeVox.verbosity == cvox.VERBOSITY_VERBOSE) {
    ret[ret.length - 1].annotation = 'math';
  }
  ret[0].pushEarcon(cvox.Earcon.MATH);
  return ret;
};
