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
  VIEWER: 'viewer',
  METADATA: 'metadata',
};

/**
 * Defines the content and buttons for each page mode.
 * `target` can be 'runtime' to send a message to the background script, or
 * 'tab' to send a message to the content script of the active tab.
 */
const POPUP_CONFIG = {
  [PageMode.ORIGINAL]: {
    buttons: [
      {text: 'Clone', command: 'clone-page-new', target: 'background'},
      {text: 'Readerable?', command: 'check-readerable', target: 'background'},
      {text: 'Distill', command: 'distill-page', target: 'background'},
      {text: 'Distill New', command: 'distill-page-new', target: 'background'},
      {text: 'Metadata', command: 'extract-metadata-new', target: 'background'},
    ],
  },
  [PageMode.CLONED]: {
    message: 'This is a cloned page.',
    buttons: [
      {text: 'Readerable?', command: 'check-readerable', target: 'cloned'},
      {text: 'Distill', command: 'distill-page', target: 'cloned'},
      {text: 'Distill New', command: 'distill-page-new', target: 'cloned'},
      {text: 'Metadata', command: 'extract-metadata-new', target: 'cloned'},
      {text: 'Boxify', command: 'boxify-dom', target: 'cloned'},
      {text: 'Visualize', command: 'visualize-readability', target: 'cloned'},
    ],
  },
  [PageMode.VIEWER]: {
    message: 'This is a distilled page.',
    buttons: [],
  },
  [PageMode.METADATA]: {
    message: 'This is a metadata page.',
    buttons: [],
  },
};


/**
 * Creates a button element with the given text and click handler.
 * @param {string} text The text to display on the button.
 * @param {function} onClick The function to call when the button is clicked.
 * @return {HTMLButtonElement} The created button element.
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
      const message = {command, target, tabId};
      if (target === 'background') {
        chrome.runtime.sendMessage(message, messageCallback);
      } else {  // target === 'cloned'
        chrome.tabs.sendMessage(tabId, message, messageCallback);
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
  const viewerUrl = chrome.runtime.getURL('viewer.html');
  const metadataUrl = chrome.runtime.getURL('metadata.html');

  let pageMode = PageMode.ORIGINAL;
  if (tab.url.startsWith(clonedUrl)) {
    pageMode = PageMode.CLONED;
  } else if (tab.url.startsWith(viewerUrl)) {
    pageMode = PageMode.VIEWER;
  } else if (tab.url.startsWith(metadataUrl)) {
    pageMode = PageMode.METADATA;
  }

  const container = document.getElementById('popup-content');
  renderPopup(container, pageMode, tab.id);
});
