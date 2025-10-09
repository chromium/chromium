// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An enum for the different modes the extension's popup can be in,
 * depending on the current page.
 * @enum {string}
 */
const PageMode = {
  ORIGINAL: 'original',
  CLONED: 'cloned',
  // Other modes will be added in subsequent CLs.
};

/**
 * Defines the content and buttons for each page mode.
 * `target` can be 'runtime' to send a message to the background script, or
 * 'tab' to send a message to the content script of the active tab.
 */
const POPUP_CONFIG = {
  [PageMode.ORIGINAL]: {
    buttons: [
      {text: 'Clone', command: 'clone-new', target: 'runtime'},
      {text: 'Readerable?', command: 'check-readerable', target: 'runtime'},
    ],
  },
  [PageMode.CLONED]: {
    message: 'This is a cloned page.',
    buttons: [
      {text: 'Readerable?', command: 'check-readerable', target: 'tab'},
    ],
  },
};


/**
 * Creates a button element with the given text and click handler.
 * @param {string} text The text to display on the button.
 * @param {function} onClick The function to call when the button is clicked.
 * @returns {HTMLButtonElement} The created button element.
 */
function createButton(text, onClick) {
  const button = document.createElement('button');
  button.textContent = text;
  button.addEventListener('click', onClick);
  return button;
}

/**
 * Renders the appropriate content in the popup based on the page mode.
 * @param {HTMLElement} container The container to render the content in.
 * @param {PageMode} pageMode The mode of the current page.
 * @param {number} tabId The ID of the active tab.
 */
function renderPopup(container, pageMode, tabId) {
  // Clear any existing content.
  container.innerHTML = '';

  const config = POPUP_CONFIG[pageMode] ?? POPUP_CONFIG[PageMode.ORIGINAL];

  if (config.message) {
    const info = document.createElement('p');
    info.textContent = config.message;
    container.appendChild(info);
  }

  config.buttons.forEach(({text, command, target}) => {
    let messageCallback;
    if (command === 'check-readerable') {
      messageCallback = (result) => {
        container.innerHTML = '';
        const info = document.createElement('p');
        info.textContent = `Readerable: ${result}`;
        container.appendChild(info);
      };
    } else {
      messageCallback = () => {
        // The callback confirms the message was received, so now we can
        // close.
        window.close();
      };
    }

    const action = () => {
      if (target === 'runtime') {
        chrome.runtime.sendMessage({command, tabId}, messageCallback);
      } else {  // target === 'tab'
        chrome.tabs.sendMessage(tabId, {command}, messageCallback);
      }
    };

    container.appendChild(createButton(text, action));
  });
}

/******** Main Execution ********/

chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
  const tab = tabs[0];
  if (!tab || !tab.url) {
    return;
  }

  const clonedUrl = chrome.runtime.getURL('cloned.html');
  let pageMode = PageMode.ORIGINAL;
  if (tab.url.startsWith(clonedUrl)) {
    pageMode = PageMode.CLONED;
  }
  // TODO(crrev.com/449799192): Add logic to detect other page modes (e.g.,
  // viewer).

  const container = document.getElementById('popup-content');
  renderPopup(container, pageMode, tab.id);
});
