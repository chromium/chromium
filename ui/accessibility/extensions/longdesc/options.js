/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Saves options to chrome.storage
function save_options() {
  var border = document.getElementById('border').checked;
  chrome.storage.sync.set({
    addBorder: border
  }, function() {
    // Update status to let user know options were saved.
    var status = document.getElementById('status');
    status.textContent = 'Options saved.';
    setTimeout(function() {
      status.textContent = '';
    }, 750);
  });
}

// Restores select box and checkbox state using the preferences
// stored in chrome.storage.
function restore_options() {
  // Use default value addBOrder = false.
  chrome.storage.sync.get("addBorder", function(item) {
    document.getElementById('border').checked = item.addBorder;
  });
}

document.addEventListener('DOMContentLoaded', restore_options);
document.getElementById('border').addEventListener('click', save_options);

document.title = chrome.i18n.getMessage('longdesc_options');
var i18nElements = document.querySelectorAll('*[i18n-content]');
for (var i = 0; i < i18nElements.length; i++) {
  var elem = i18nElements[i];
  var msg = elem.getAttribute('i18n-content');
  elem.innerHTML = chrome.i18n.getMessage(msg);
}
