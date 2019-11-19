// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of live regions on the page and speaks updates
 * when they change.
 *
 */

goog.provide('cvox.LiveRegions');

goog.require('cvox.AriaUtil');
goog.require('cvox.ChromeVox');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.Interframe');
goog.require('cvox.NavDescription');
goog.require('cvox.NavigationSpeaker');

/**
 * @constructor
 */
cvox.LiveRegions = function() {
};

/**
 * @type {Date}
 */
cvox.LiveRegions.pageLoadTime = null;

/**
 * Time in milliseconds after initial page load to ignore live region
 * updates, to avoid announcing regions as they're initially created.
 * The exception is alerts, they're announced when a page is loaded.
 * @type {number}
 * @const
 */
cvox.LiveRegions.INITIAL_SILENCE_MS = 2000;

/**
 * Time in milliseconds to wait for a node to become visible after a
 * mutation. Needed to allow live regions to fade in and have an initial
 * opacity of zero.
 * @type {number}
 * @const
 */
cvox.LiveRegions.VISIBILITY_TIMEOUT_MS = 50;

/**
 * A mapping from announced text to the time it was last spoken.
 * @type {Object<Date>}
 */
cvox.LiveRegions.lastAnnouncedMap = {};

/**
 * Maximum time interval in which to discard duplicate live region announcement.
 * @type {number}
 * @const
 */
cvox.LiveRegions.MAX_DISCARD_DUPS_MS = 2000;

/**
 * @type {Date}
*/
cvox.LiveRegions.lastAnnouncedTime = null;

/**
 * Tracks nodes handled during mutation processing.
 * @type {!Array<Node>}
 */
cvox.LiveRegions.nodesAlreadyHandled = [];

/**
 * @param {Date} pageLoadTime The time the page was loaded. Live region
 *     updates within the first INITIAL_SILENCE_MS milliseconds are ignored.
 * @param {cvox.QueueMode} queueMode Interrupt or flush.  Polite live region
 *   changes always queue.
 * @param {boolean} disableSpeak true if change announcement should be disabled.
 * @return {boolean} true if any regions announced.
 */
cvox.LiveRegions.init = function(pageLoadTime, queueMode, disableSpeak) {
  cvox.LiveRegions.pageLoadTime = pageLoadTime;

  if (disableSpeak || !cvox.ChromeVox.documentHasFocus()) {
    return false;
  }

  // Speak any live regions already on the page. The logic below will
  // make sure that only alerts are actually announced.
  var anyRegionsAnnounced = false;
  var regions = cvox.AriaUtil.getLiveRegions(document.body);
  for (var i = 0; i < regions.length; i++) {
    cvox.LiveRegions.handleOneChangedNode(
        regions[i],
        regions[i],
        false,
        false,
        function(assertive, navDescriptions) {
          if (!assertive && queueMode == cvox.QueueMode.FLUSH) {
            queueMode = cvox.QueueMode.QUEUE;
          }
          var descSpeaker = new cvox.NavigationSpeaker();
          descSpeaker.speakDescriptionArray(navDescriptions, queueMode, null);
          anyRegionsAnnounced = true;
        });
  }

  cvox.Interframe.addListener(function(message) {
    if (message['command'] != 'speakLiveRegion') {
      return;
    }
    var iframes = document.getElementsByTagName('iframe');
    for (var i = 0, iframe; iframe = iframes[i]; i++) {
      if (iframe.src == message['src']) {
        if (!cvox.DomUtil.isVisible(iframe)) {
          return;
        }
        var structs = JSON.parse(message['content']);
        var descriptions = [];
        for (var j = 0, description; description = structs[j]; j++) {
          descriptions.push(new cvox.NavDescription(description));
        }
        new cvox.NavigationSpeaker()
            .speakDescriptionArray(descriptions, message['queueMode'], null);
      }
    }
  });

  return anyRegionsAnnounced;
};

/**
 * See if any mutations pertain to a live region, and speak them if so.
 *
 * This function is not reentrant, it uses some global state to keep
 * track of nodes it's already spoken once.
 *
 * @param {Array<MutationRecord>} mutations The mutations.
 * @param {function(boolean, Array<cvox.NavDescription>)} handler
 *     A callback function that handles each live region description found.
 *     The function is passed a boolean indicating if the live region is
 *     assertive, and an array of navdescriptions to speak.
 */
cvox.LiveRegions.processMutations = function(mutations, handler) {
  cvox.LiveRegions.nodesAlreadyHandled = [];
  mutations.forEach(function(mutation) {
    if (mutation.target.hasAttribute &&
        mutation.target.hasAttribute('cvoxIgnore')) {
      return;
    }
    if (mutation.addedNodes) {
      for (var i = 0; i < mutation.addedNodes.length; i++) {
        if (mutation.addedNodes[i].hasAttribute &&
            mutation.addedNodes[i].hasAttribute('cvoxIgnore')) {
          continue;
        }
        cvox.LiveRegions.handleOneChangedNode(
            mutation.addedNodes[i], mutation.target, false, true, handler);
      }
    }
    if (mutation.removedNodes) {
      for (var i = 0; i < mutation.removedNodes.length; i++) {
        if (mutation.removedNodes[i].hasAttribute &&
            mutation.removedNodes[i].hasAttribute('cvoxIgnore')) {
          continue;
        }
        cvox.LiveRegions.handleOneChangedNode(
            mutation.removedNodes[i], mutation.target, true, false, handler);
      }
    }
    if (mutation.type == 'characterData') {
      cvox.LiveRegions.handleOneChangedNode(
          mutation.target, mutation.target, false, false, handler);
    }
    if (mutation.attributeName == 'class' ||
        mutation.attributeName == 'style' ||
        mutation.attributeName == 'hidden') {
      var attr = mutation.attributeName;
      var target = mutation.target;
      var newInvisible = !cvox.DomUtil.isVisible(target);

      // Create a fake element on the page with the old values of
      // class, style, and hidden for this element, to see if that test
      // element would have had different visibility.
      var testElement = document.createElement('div');
      testElement.setAttribute('cvoxIgnore', '1');
      testElement.setAttribute('class', target.getAttribute('class'));
      testElement.setAttribute('style', target.getAttribute('style'));
      testElement.setAttribute('hidden', target.getAttribute('hidden'));
      testElement.setAttribute(attr, /** @type {string} */ (mutation.oldValue));

      var oldInvisible = true;
      if (target.parentElement) {
        target.parentElement.appendChild(testElement);
        oldInvisible = !cvox.DomUtil.isVisible(testElement);
        target.parentElement.removeChild(testElement);
      } else {
        oldInvisible = !cvox.DomUtil.isVisible(testElement);
      }

      if (oldInvisible === true && newInvisible === false) {
        cvox.LiveRegions.handleOneChangedNode(
            mutation.target, mutation.target, false, true, handler);
      } else if (oldInvisible === false && newInvisible === true) {
        cvox.LiveRegions.handleOneChangedNode(
            mutation.target, mutation.target, true, false, handler);
      }
    }
  });
  cvox.LiveRegions.nodesAlreadyHandled.length = 0;
};

/**
 * Handle one changed node. First check if this node is itself within
 * a live region, and if that fails see if there's a live region within it
 * and call this method recursively. For each actual live region, call a
 * method to recursively announce all changes.
 *
 * @param {Node} node A node that's changed.
 * @param {Node} parent The parent node.
 * @param {boolean} isRemoval True if this node was removed.
 * @param {boolean} subtree True if we should check the subtree.
 * @param {function(boolean, Array<cvox.NavDescription>)} handler
 *     Callback function to be called for each live region found.
 */
cvox.LiveRegions.handleOneChangedNode = function(
    node, parent, isRemoval, subtree, handler) {
  var liveRoot = isRemoval ? parent : node;
  if (!(liveRoot instanceof Element)) {
    liveRoot = liveRoot.parentElement;
  }
  while (liveRoot) {
    if (cvox.AriaUtil.getAriaLive(liveRoot)) {
      break;
    }
    liveRoot = liveRoot.parentElement;
  }
  if (!liveRoot) {
    if (subtree && node != document.body) {
      var subLiveRegions = cvox.AriaUtil.getLiveRegions(node);
      for (var i = 0; i < subLiveRegions.length; i++) {
        cvox.LiveRegions.handleOneChangedNode(
            subLiveRegions[i], parent, isRemoval, false, handler);
      }
    }
    return;
  }

  // If the page just loaded and this is any region type other than 'alert',
  // skip it. Alerts are the exception, they're announced on page load.
  var deltaTime = new Date() - cvox.LiveRegions.pageLoadTime;
  if (cvox.AriaUtil.getRoleAttribute(liveRoot) != 'alert' &&
      deltaTime < cvox.LiveRegions.INITIAL_SILENCE_MS) {
    return;
  }

  if (cvox.LiveRegions.nodesAlreadyHandled.indexOf(node) >= 0) {
    return;
  }
  cvox.LiveRegions.nodesAlreadyHandled.push(node);

  if (cvox.AriaUtil.getAriaBusy(liveRoot)) {
    return;
  }

  if (isRemoval) {
    if (!cvox.AriaUtil.getAriaRelevant(liveRoot, 'removals')) {
      return;
    }
  } else {
    if (!cvox.AriaUtil.getAriaRelevant(liveRoot, 'additions')) {
      return;
    }
  }

  cvox.LiveRegions.announceChangeIfVisible(node, liveRoot, isRemoval, handler);
};

/**
 * Announce one node within a live region if it's visible.
 * In order to handle live regions that fade in, if the node isn't currently
 * visible, check again after a short timeout.
 *
 * @param {Node} node A node in a live region.
 * @param {Node} liveRoot The root of the live region this node is in.
 * @param {boolean} isRemoval True if this node was removed.
 * @param {function(boolean, Array<cvox.NavDescription>)} handler
 *     Callback function to be called for each live region found.
 */
cvox.LiveRegions.announceChangeIfVisible = function(
    node, liveRoot, isRemoval, handler) {
  if (cvox.DomUtil.isVisible(liveRoot)) {
    cvox.LiveRegions.announceChange(node, liveRoot, isRemoval, handler);
  } else {
    window.setTimeout(function() {
      if (cvox.DomUtil.isVisible(liveRoot)) {
        cvox.LiveRegions.announceChange(node, liveRoot, isRemoval, handler);
      }
    }, cvox.LiveRegions.VISIBILITY_TIMEOUT_MS);
  }
};

/**
 * Announce one node within a live region.
 *
 * @param {Node} node A node in a live region.
 * @param {Node} liveRoot The root of the live region this node is in.
 * @param {boolean} isRemoval True if this node was removed.
 * @param {function(boolean, Array<cvox.NavDescription>)} handler
 *     Callback function to be called for each live region found.
 */
cvox.LiveRegions.announceChange = function(
    node, liveRoot, isRemoval, handler) {
  // If this node is in an atomic container, announce the whole container.
  // This includes aria-atomic, but also ARIA controls and other nodes
  // whose ARIA roles make them leaves.
  if (node != liveRoot) {
    var atomicContainer = node.parentElement;
    while (atomicContainer) {
      if ((cvox.AriaUtil.getAriaAtomic(atomicContainer) ||
           cvox.AriaUtil.isLeafElement(atomicContainer) ||
           cvox.AriaUtil.isControlWidget(atomicContainer)) &&
          !cvox.AriaUtil.isCompositeControl(atomicContainer)) {
        node = atomicContainer;
      }
      if (atomicContainer == liveRoot) {
        break;
      }
      atomicContainer = atomicContainer.parentElement;
    }
  }

  var navDescriptions = cvox.LiveRegions.getNavDescriptionsRecursive(node);
  if (isRemoval) {
    navDescriptions = [cvox.DescriptionUtil.getDescriptionFromAncestors(
        [node], true, cvox.ChromeVox.verbosity)];
    navDescriptions = [new cvox.NavDescription({
      context: Msgs.getMsg('live_regions_removed'), text: ''
    })].concat(navDescriptions);
  }

  if (navDescriptions.length == 0) {
    return;
  }

  // Don't announce alerts on page load if their text and values consist of
  // just whitespace.
  var deltaTime = new Date() - cvox.LiveRegions.pageLoadTime;
  if (cvox.AriaUtil.getRoleAttribute(liveRoot) == 'alert' &&
      deltaTime < cvox.LiveRegions.INITIAL_SILENCE_MS) {
    var regionText = '';
    for (var i = 0; i < navDescriptions.length; i++) {
      regionText += navDescriptions[i].text;
      regionText += navDescriptions[i].userValue;
    }
    if (cvox.DomUtil.collapseWhitespace(regionText) == '') {
      return;
    }
  }

  // First, evict expired entries.
  var now = new Date();
  for (var announced in cvox.LiveRegions.lastAnnouncedMap) {
    if (now - cvox.LiveRegions.lastAnnouncedMap[announced] >
        cvox.LiveRegions.MAX_DISCARD_DUPS_MS) {
      delete cvox.LiveRegions.lastAnnouncedMap[announced];
    }
  }

  // Then, skip announcement if it was already spoken in the past 2000 ms.
  var key = navDescriptions.reduce(function(prev, navDescription) {
    return prev + '|' + navDescription.text;
  }, '');

  if (cvox.LiveRegions.lastAnnouncedMap[key]) {
    return;
  }
  cvox.LiveRegions.lastAnnouncedMap[key] = now;

  var assertive = cvox.AriaUtil.getAriaLive(liveRoot) == 'assertive';
  if (cvox.Interframe.isIframe() && !cvox.ChromeVox.documentHasFocus()) {
    cvox.Interframe.sendMessageToParentWindow(
        {'command': 'speakLiveRegion',
         'content': JSON.stringify(navDescriptions),
         'queueMode': assertive ? 0 : 1,
         'src': window.location.href }
        );
    return;
  }

  // Set a category on the NavDescriptions - that way live regions
  // interrupt other live regions but not anything else.
  navDescriptions.forEach(function(desc) {
    if (!desc.category) {
      desc.category = cvox.TtsCategory.LIVE;
    }
  });

  // TODO(dmazzoni): http://crbug.com/415679 Temporary design decision;
  // until we have a way to tell the speech queue to group the nav
  // descriptions together, collapse them into one.
  // Otherwise, one nav description could be spoken, then something unrelated,
  // then the rest.
  if (navDescriptions.length > 1) {
    var allStrings = [];
    navDescriptions.forEach(function(desc) {
      if (desc.context) {
        allStrings.push(desc.context);
      }
      if (desc.text) {
        allStrings.push(desc.text);
      }
      if (desc.userValue) {
        allStrings.push(desc.userValue);
      }
    });
    navDescriptions = [new cvox.NavDescription({
      text: allStrings.join(', '),
      category: cvox.TtsCategory.LIVE
    })];
  }

  handler(assertive, navDescriptions);
};

/**
 * Recursively build up the value of a live region and return it as
 * an array of NavDescriptions. Each atomic portion of the region gets a
 * single string, otherwise each leaf node gets its own string.
 *
 * @param {Node} node A node in a live region.
 * @return {Array<cvox.NavDescription>} An array of NavDescriptions
 *     describing atomic nodes or leaf nodes in the subtree rooted
 *     at this node.
 */
cvox.LiveRegions.getNavDescriptionsRecursive = function(node) {
  if (cvox.AriaUtil.getAriaAtomic(node) ||
      cvox.DomUtil.isLeafNode(node)) {
    var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
        [node], true, cvox.ChromeVox.verbosity);
    if (!description.isEmpty()) {
      return [description];
    } else {
      return [];
    }
  }
  return cvox.DescriptionUtil.getFullDescriptionsFromChildren(null,
      /** @type {!Element} */ (node));
};
