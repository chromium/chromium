// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JS helpers used on login.
 */

cr.define('cr.ui.LoginUITools', function() {
  return {
    /**
     * Computes max-height for an element so that it doesn't overlap shelf.
     * @param {Element} element DOM element
     * @param {boolean} wholeWindow Whether the element can go outside
     *                  outer-container.
     */
    getMaxHeightBeforeShelfOverlapping : function(element, wholeWindow) {
      var maxAllowedHeight =
          $('outer-container').offsetHeight -
          element.getBoundingClientRect().top -
          parseInt(window.getComputedStyle(element).marginTop, 10) -
          parseInt(window.getComputedStyle(element).marginBottom, 10);
      if (wholeWindow) {
        maxAllowedHeight +=
           parseInt(window.getComputedStyle($('outer-container')).bottom, 10);
      }
      return maxAllowedHeight;
    },

    /**
     * Computes max-width for an element so that it does fit the
     * outer-container.
     * @param {Element} element DOM element
     */
    getMaxWidthToFit : function(element) {
      var maxAllowedWidth =
          $('outer-container').offsetWidth -
          element.getBoundingClientRect().left -
          parseInt(window.getComputedStyle(element).marginLeft, 10) -
          parseInt(window.getComputedStyle(element).marginRight, 10);
      return maxAllowedWidth;
    },

    /**
     * Listens to key events on input element.
     * @param {Element} element DOM element
     * @param {Object} callback
     */
    addSubmitListener : function(element, callback) {
      element.addEventListener('keydown', (function(callback, e) {
        if (e.keyCode != 13)
          return;
        callback();
      }).bind(undefined, callback));
    },
  }

});
