// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SESSION_STORAGE_KEY = 'clonedPageData';

/**
 * Retrieves the cloned page data, handling both initial loads and reloads.
 * On initial load, it fetches from chrome.storage.session using the URL's 'id'
 * parameter, and then caches the data to sessionStorage. On reloads, it
 * retrieves the data directly from sessionStorage.
 * @returns {Promise<object|null>} A promise that resolves to an object
 *     containing the {html, styles} of the cloned page, or null on error.
 */
async function getClonedPageData() {
  // On refresh, the data will be in sessionStorage.
  const cachedData = sessionStorage.getItem(SESSION_STORAGE_KEY);
  if (cachedData) {
    return JSON.parse(cachedData);
  }

  // On initial load, the data is in chrome.storage.session.
  const id = consumeIdFromUrl();
  const data = await consumeDataFromChromeStorage(id);
  if (!data?.html) {
    return null;
  }

  // Store the page content in sessionStorage so it can be restored on refresh.
  sessionStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(data));

  return data;
}

/**
 * Renders the cloned page content into the document.
 * @param {string} html The HTML content of the page.
 * @param {object} styles The computed styles from the original page.
 */
function renderClonedPage(html, styles) {
  const parser = new DOMParser();
  const newDoc = parser.parseFromString(html, 'text/html');

  // Apply the original page's root and body styles to the new document.
  newDoc.documentElement.style.fontSize = styles.rootFontSize;
  newDoc.body.style.fontSize = styles.bodyFontSize;
  newDoc.body.style.backgroundColor = styles.bodyBackgroundColor;
  newDoc.body.style.color = styles.bodyColor;

  // Replace the current document with the cloned one.
  document.documentElement.replaceWith(newDoc.documentElement);
}

/******** Event Listeners ********/

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.command === 'check-readerable') {
    loadAndRunReaderableCheck((isReaderable) => {
      sendResponse(isReaderable);
    });
    return true;
  } else if (request.command === 'distill') {
    loadAndRunReadabilityAndRender((renderedHtml) => {
      if (renderedHtml) {
        document.documentElement.innerHTML = renderedHtml;
      }
      sendResponse({});
    });
    return true;
  } else if (request.command === 'distill-new') {
    loadAndRunReadabilityAndRender((renderedHtml) => {
      if (renderedHtml) {
        chrome.runtime.sendMessage(
            {command: 'show-distilled-new', data: {html: renderedHtml}});
      }
      sendResponse({});
    });
    return true;
  }
});

/**
 * Loads Readability and the processor to get the final rendered HTML.
 * @param {function(string)} callback The function to call with the rendered
 *     HTML.
 */
function loadAndRunReadabilityAndRender(callback) {
  const run = async () => {
    const renderedHtml =
        await window.ReadabilityExtension.processAndRenderArticle(document);
    callback(renderedHtml);
  };

  if (typeof Readability !== 'undefined' &&
      typeof window.ReadabilityExtension.processAndRenderArticle !== 'undefined') {
    run();
    return;
  }

  // The distillation scripts are not pre-loaded in cloned.html for performance.
  // This fallback dynamically injects them on the first run.
  const script = document.createElement('script');
  script.src = chrome.runtime.getURL('Readability.js');
  script.onload = () => {
    const processorScript = document.createElement('script');
    processorScript.src = chrome.runtime.getURL('article_processor.js');
    processorScript.onload = run;
    document.head.appendChild(processorScript);
  };
  document.head.appendChild(script);
}


/**
 * Loads and runs the isProbablyReaderable() check.
 * @param {function(boolean)} callback The function to call with the result.
 */
function loadAndRunReaderableCheck(callback) {
  const run = () => {
    const isReaderable = isProbablyReaderable(document);
    callback(isReaderable);
  };

  if (typeof isProbablyReaderable !== 'undefined') {
    run();
    return;
  }

  const script = document.createElement('script');
  script.src = chrome.runtime.getURL('Readability-readerable.js');
  script.onload = run;
  document.head.appendChild(script);
}

/******** Main Initialization ********/

(async () => {
  const data = await getClonedPageData();
  if (data) {
    renderClonedPage(data.html, data.styles);
  }
})();
