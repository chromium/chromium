// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Uses ChromeVox API to enhance the search experience.
 */

goog.provide('cvox.Search');

goog.require('cvox.ChromeVox');
goog.require('cvox.SearchConstants');
goog.require('cvox.SearchResults');
goog.require('cvox.SearchUtil');
goog.require('cvox.UnknownResult');

/**
 * @constructor
 */
cvox.Search = function() {
};

/**
 * Selectors to match results.
 * @type {Object<string>}
 */
cvox.Search.selectors = {};

/**
 * Selectors for web results.
 */
cvox.Search.webSelectors = {
  /* Topstuff typically contains important messages to be added first. */
  TOPSTUFF_SELECT: '#topstuff',
  SPELL_SUGG_SELECT: '.ssp',
  SPELL_CORRECTION_SELECT: '.sp_cnt',
  KNOW_PANEL_SELECT: '.knop',
  RESULT_SELECT: '.g',
  RELATED_SELECT: '#brs'
};

/**
 * Selectors for image results.
 */
cvox.Search.imageSelectors = {
  IMAGE_CATEGORIES_SELECT: '#ifbc .rg_fbl',
  IMAGE_RESULT_SELECT: '#rg_s .rg_di'
};

/**
 * Index of the currently synced result.
 * @type {number}
 */
cvox.Search.index;

/**
 * Array of the search results.
 * @type {Array<Node>}
 */
cvox.Search.results = [];

/**
 * Array of the navigation panes.
 * @type {Array<Element>}
 */
cvox.Search.panes = [];

/**
 * Index of the currently synced pane.
 * @type {number}
 */
cvox.Search.paneIndex;

/**
 * If currently synced item is a pane.
 */
cvox.Search.isPane = false;

/**
 * Class of a selected pane.
 */
cvox.Search.SELECTED_PANE_CLASS = 'hdtb_mitem hdtb_msel';


/**
 * Speak and sync.
 * @private
 */
cvox.Search.speakSync_ = function() {
  var result = cvox.Search.results[cvox.Search.index];
  var resultType = cvox.Search.getResultType(result);
  var isSpoken = resultType.speak(result);
  cvox.ChromeVox.syncToNode(resultType.getSyncNode(result), !isSpoken);
  cvox.Search.isPane = false;
};

/**
 * Sync the search result index to ChromeVox.
 */
cvox.Search.syncToIndex = function() {
  cvox.ChromeVox.tts.stop();
  var prop = { endCallback: cvox.Search.speakSync_ };
  if (cvox.Search.index === 0) {
    cvox.ChromeVox.tts.speak('First result', cvox.QueueMode.QUEUE, prop);
  } else if (cvox.Search.index === cvox.Search.results.length - 1) {
    cvox.ChromeVox.tts.speak('Last result', cvox.QueueMode.QUEUE, prop);
  } else {
    cvox.Search.speakSync_();
  }
};

/**
 * Sync the current pane index to ChromeVox.
 */
cvox.Search.syncPaneToIndex = function() {
  var pane = cvox.Search.panes[cvox.Search.paneIndex];
  var anchor = pane.querySelector('a');
  if (anchor) {
    cvox.ChromeVox.syncToNode(anchor, true);
  } else {
    cvox.ChromeVox.syncToNode(pane, true);
  }
  cvox.Search.isPane = true;
};

/**
 * Get the type of the result such as Knowledge Panel, Weather, etc.
 * @param {Node} result Result to be classified.
 * @return {cvox.AbstractResult} Type of the result.
 */
cvox.Search.getResultType = function(result) {
  for (var i = 0; i < cvox.SearchResults.RESULT_TYPES.length; i++) {
    var resultType = new cvox.SearchResults.RESULT_TYPES[i]();
    if (resultType.isType(result)) {
      return resultType;
    }
  }
  return new cvox.UnknownResult();
};

/**
 * Get the page number associated with the url.
 * @param {string} url Url of search page.
 * @return {number} Page number.
 */
cvox.Search.getPageNumber = function(url) {
  var PAGE_ANCHOR_SELECTOR = '#nav .fl';
  var pageAnchors = document.querySelectorAll(PAGE_ANCHOR_SELECTOR);
  for (var i = 0; i < pageAnchors.length; i++) {
    var pageAnchor = pageAnchors.item(i);
    if (pageAnchor.href === url) {
      return parseInt(pageAnchor.innerText, 10);
    }
  }
  return NaN;
};

/**
 * Navigate to the next / previous page.
 * @param {boolean} next True for the next page, false for the previous.
 */
cvox.Search.navigatePage = function(next) {
  /* NavEnd contains previous / next page links. */
  var NAV_END_CLASS = 'navend';
  var navEnds = document.getElementsByClassName(NAV_END_CLASS);
  var navEnd = next ? navEnds[1] : navEnds[0];
  var url = cvox.SearchUtil.extractURL(navEnd);
  var navToUrl = function() {
    window.location = url;
  };
  var prop = { endCallback: navToUrl };
  if (url) {
    var pageNumber = cvox.Search.getPageNumber(url);
    if (!isNaN(pageNumber)) {
      cvox.ChromeVox.tts.speak('Page ' + pageNumber, cvox.QueueMode.FLUSH,
                               prop);
    } else {
      cvox.ChromeVox.tts.speak('Unknown page.', cvox.QueueMode.FLUSH, prop);
    }
  }
};

/**
 * Navigates to the currently synced pane.
 */
cvox.Search.goToPane = function() {
  var pane = cvox.Search.panes[cvox.Search.paneIndex];
  if (pane.className === cvox.Search.SELECTED_PANE_CLASS) {
    cvox.ChromeVox.tts.speak('You are already on that page.',
                             cvox.QueueMode.QUEUE);
    return;
  }
  var anchor = pane.querySelector('a');
  cvox.ChromeVox.tts.speak(anchor.textContent, cvox.QueueMode.QUEUE);
  var url = cvox.SearchUtil.extractURL(pane);
  if (url) {
    window.location = url;
  }
};

/**
 * Follow the link to the current result.
 */
cvox.Search.goToResult = function() {
  var result = cvox.Search.results[cvox.Search.index];
  var resultType = cvox.Search.getResultType(result);
  var url = resultType.getURL(result);
  if (url) {
    window.location = url;
  }
};

/**
 * Handle the keyboard.
 * @param {Event} evt Keydown event.
 * @return {boolean} True if key was handled, false otherwise.
 */
cvox.Search.keyhandler = function(evt) {
  var SEARCH_INPUT_ID = 'gbqfq';
  var searchInput = document.getElementById(SEARCH_INPUT_ID);
  var result = cvox.Search.results[cvox.Search.index];
  var ret = false;

  /* TODO(peterxiao): Add cvox api call to determine cvox key. */
  if (evt.shiftKey || evt.altKey || evt.ctrlKey) {
    return false;
  }

  /* Do not handle if search input has focus, or if the search widget
   * has focus.
   */
  if (document.activeElement !== searchInput &&
      !cvox.SearchUtil.isSearchWidgetActive()) {
    switch (evt.keyCode) {
    case cvox.SearchConstants.KeyCode.UP:
      /* Add results.length because JS Modulo is silly. */
      cvox.Search.index = cvox.SearchUtil.subOneWrap(cvox.Search.index,
        cvox.Search.results.length);
      if (cvox.Search.index === cvox.Search.results.length - 1) {
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
      }
      cvox.Search.syncToIndex();
      break;

    case cvox.SearchConstants.KeyCode.DOWN:
      cvox.Search.index = cvox.SearchUtil.addOneWrap(cvox.Search.index,
        cvox.Search.results.length);
      if (cvox.Search.index === 0) {
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
      }
      cvox.Search.syncToIndex();
      break;

    case cvox.SearchConstants.KeyCode.PAGE_UP:
      cvox.Search.navigatePage(false);
      break;

    case cvox.SearchConstants.KeyCode.PAGE_DOWN:
      cvox.Search.navigatePage(true);
      break;

    case cvox.SearchConstants.KeyCode.LEFT:
      cvox.Search.paneIndex = cvox.SearchUtil.subOneWrap(cvox.Search.paneIndex,
        cvox.Search.panes.length);
      cvox.Search.syncPaneToIndex();
      break;

    case cvox.SearchConstants.KeyCode.RIGHT:
      cvox.Search.paneIndex = cvox.SearchUtil.addOneWrap(cvox.Search.paneIndex,
        cvox.Search.panes.length);
      cvox.Search.syncPaneToIndex();
      break;

    case cvox.SearchConstants.KeyCode.ENTER:
      if (cvox.Search.isPane) {
        cvox.Search.goToPane();
      } else {
        cvox.Search.goToResult();
      }
      break;

    default:
      return false;
    }
    evt.preventDefault();
    evt.stopPropagation();
    return true;
  }
  return false;
};

/**
 * Adds the elements that match the selector to results.
 * @param {string} selector Selector of element to add.
 */
cvox.Search.addToResultsBySelector = function(selector) {
  var nodes = document.querySelectorAll(selector);
  for (var i = 0; i < nodes.length; i++) {
    var node = nodes.item(i);
    /* Do not add if empty. */
    if (node.innerHTML !== '') {
      cvox.Search.results.push(nodes.item(i));
    }
  }
};

/**
 * Populates the panes array.
 */
cvox.Search.populatePanes = function() {
  cvox.Search.panes = [];
  var PANE_SELECT = '.hdtb_mitem';
  var paneElems = document.querySelectorAll(PANE_SELECT);
  for (var i = 0; i < paneElems.length; i++) {
    cvox.Search.panes.push(paneElems.item(i));
  }
};

/**
 * Populates the results with results.
 */
cvox.Search.populateResults = function() {
  for (var prop in cvox.Search.selectors) {
    cvox.Search.addToResultsBySelector(cvox.Search.selectors[prop]);
  }
};

/**
 * Populates the results with ad results.
 */
cvox.Search.populateAdResults = function() {
  cvox.Search.results = [];
  var ADS_SELECT = '.ads-ad';
  cvox.Search.addToResultsBySelector(ADS_SELECT);
};

/**
 * Observes mutations and updates results accordingly.
 */
cvox.Search.observeMutation = function() {
  var SEARCH_AREA_SELECT = '#rg_s';
  var target = document.querySelector(SEARCH_AREA_SELECT);

  var observer = new MutationObserver(function(mutations) {
    cvox.Search.results = [];
    cvox.Search.populateResults();
  });

  var config =
      /** @type MutationObserverInit */
      ({ attributes: true, childList: true, characterData: true });
  observer.observe(target, config);
};

/**
 * Get the current selected pane's index.
 * @return {number} Index of selected pane.
 */
cvox.Search.getSelectedPaneIndex = function() {
  var panes = cvox.Search.panes;
  for (var i = 0; i < panes.length; i++) {
    if (panes[i].className === cvox.Search.SELECTED_PANE_CLASS) {
      return i;
    }
  }
  return 0;
};

/**
 * Get the ancestor of node that is a result.
 * @param {Node} node Node.
 * @return {Node} Result ancestor.
 */
cvox.Search.getAncestorResult = function(node) {
  var curr = node;
  while (curr) {
    for (var prop in cvox.Search.selectors) {
      var selector = cvox.Search.selectors[prop];
      if (curr.webkitMatchesSelector && curr.webkitMatchesSelector(selector)) {
        return curr;
      }
    }
    curr = curr.parentNode;
  }
  return null;
};

/**
 * Sync to the correct initial node.
 */
cvox.Search.initialSync = function() {
  var currNode = cvox.ChromeVox.navigationManager.getCurrentNode();
  var result = cvox.Search.getAncestorResult(currNode);
  cvox.Search.index = cvox.Search.results.indexOf(result);
  if (cvox.Search.index === -1) {
    cvox.Search.index = 0;
  }

  if (cvox.Search.results.length > 0) {
    cvox.Search.syncToIndex();
  }
};

/**
 * Initialize Search.
 */
cvox.Search.init = function() {
  cvox.Search.index = 0;

  /* Flush out anything that may have been speaking. */
  cvox.ChromeVox.tts.stop();

  /* Determine the type of search. */
  var SELECTED_CLASS = 'hdtb-msel';
  var selected = document.getElementsByClassName(SELECTED_CLASS)[0];
  if (!selected) {
    return;
  }

  var selectedHTML = selected.innerHTML;
  switch (selectedHTML) {
  case 'Web':
  case 'News':
    cvox.Search.selectors = cvox.Search.webSelectors;
    break;
  case 'Images':
    cvox.Search.selectors = cvox.Search.imageSelectors;
    cvox.Search.observeMutation();
    break;
  default:
    return;
  }

  cvox.Search.populateResults();
  cvox.Search.populatePanes();
  cvox.Search.paneIndex = cvox.Search.getSelectedPaneIndex();

  cvox.Search.initialSync();

};
