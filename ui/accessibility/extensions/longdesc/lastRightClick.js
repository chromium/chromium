/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

var borderColor;
var borderStyle;
var borderWidth;

chrome.storage.onChanged.addListener(function(changes, namespace) {
  if (changes.addBorder.newValue) {
    addBorders();
  } else {
    removeBorders();
  }
});

chrome.storage.sync.get("addBorder", function(item) {
  if (item.addBorder) {
    addBorders();
  }
});

document.addEventListener('contextmenu', function(element) {
  updateContextMenuItem(element);
}, false);

document.addEventListener('mouseover', function(element) {
  updateContextMenuItem(element);
}, false);

document.addEventListener('focus', function(element) {
  updateContextMenuItem(element);
});

/**
 * Sends a message to the backgrond script notifying it to
 * enable or disable the context menu item.
 *
 * @param element
 */
function updateContextMenuItem(element) {
  var longDesc = '';
  var ariaDescribedAt = '';

  if (element.target.hasAttribute("longdesc")) {
    longDesc = element.target.getAttribute("longdesc");
    var link = document.createElement("a");
    link.href = longDesc;
    longDesc = link.href;
  }

  if (element.target.hasAttribute("aria-describedat")) {
    ariaDescribedAt = element.target.getAttribute("aria-describedat");
  }

  if (longDesc !== '' || ariaDescribedAt !== '') {
    chrome.runtime.sendMessage({
      ariaDescribedAt: ariaDescribedAt,
      longDesc: longDesc,
      enabled: true
    });
  } else {
    chrome.runtime.sendMessage({
      enabled: false
    });
  }
}

/**
 * Modify border to make the HTML element more visible.
 */
function addBorders() {
  document.body.setAttribute('showlongdescborders', '');
}

/**
 * Revert back to the original border styling.
 */
function removeBorders() {
  document.body.removeAttribute('showlongdescborders', '');
}
