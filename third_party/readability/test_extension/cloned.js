// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SESSION_STORAGE_KEY = 'clonedPageData';

/**
 * Retrieves the cloned page data, handling both initial loads and reloads.
 * On initial load, it fetches from chrome.storage.session using the URL's 'id'
 * parameter, and then caches the data to sessionStorage. On reloads, it
 * retrieves the data directly from sessionStorage.
 * @return {Promise<object|null>} A promise that resolves to an object
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
  // Only respond to messages explicitly targeted at this script.
  if (request.target !== 'cloned') {
    return false;
  }

  if (request.command === 'check-readerable') {
    const isReaderable = runCheckReaderable();
    sendResponse(isReaderable);
  } else if (request.command === 'distill-page') {
    runDistillPage().then(renderedHtml => {
      if (renderedHtml) {
        document.documentElement.innerHTML = renderedHtml;
      }
      sendResponse({});
    });
    return true;
  } else if (request.command === 'distill-page-new') {
    runDistillPage().then(renderedHtml => {
      if (renderedHtml) {
        chrome.runtime.sendMessage(
            {command: 'show-distilled-new', data: {html: renderedHtml}});
      }
      sendResponse({});
    });
    return true;
  } else if (request.command === 'extract-metadata-new') {
    const article = runExtractMetadata();
    if (article) {
      chrome.runtime.sendMessage(
          {command: 'show-metadata-new', data: article});
    }
    sendResponse({});
  } else if (request.command === 'boxify-dom') {
    runBoxifyDom();
    sendResponse({});
  } else if (request.command === 'visualize-readability') {
    runVisualizeReadability();
    sendResponse({});
  }
  // Signal that the message was not handled.
  return false;
});

/**
 * Runs the isProbablyReaderable() check.
 * @return {boolean} Whether the page is readerable.
 */
function runCheckReaderable() {
  return isProbablyReaderable(document);
}

/**
 * Injects a stylesheet to draw a black outline around every element.
 */
function runBoxifyDom() {
  const styleId = 'readability-boxify-style';
  if (!document.getElementById(styleId)) {
    const style = document.createElement('style');
    style.id = styleId;
    style.textContent = `* { outline: 1px solid black !important; }`;
    document.head.appendChild(style);
  }
}

/**
 * Extracts the article metadata.
 * @return {object|null} The article metadata object, or null on failure.
 */
function runExtractMetadata() {
  return window.ReadabilityExtension.extractMetadata(document);
}

/**
 * Runs the distillation and rendering process.
 * @return {Promise<string|null>} A promise that resolves to the rendered HTML,
 *     or null on failure.
 */
function runDistillPage() {
  return window.ReadabilityExtension.processAndRenderArticle(document);
}

/**
 * Runs the full visualization process.
 */
function runVisualizeReadability() {
  const docClone = document.cloneNode(true);
  const elementMap = new Map();
  let elementCounter = 0;

  // Tag every element in the cloned document and the original document.
  docClone.querySelectorAll('*').forEach((element, index) => {
    const id = `readability-id-${elementCounter++}`;
    element.setAttribute('data-readability-id', id);
    // Find the corresponding element in the visible DOM and tag it too.
    const originalElement = document.querySelectorAll('*')[index];
    if (originalElement) {
      originalElement.setAttribute('data-readability-id', id);
      elementMap.set(id, originalElement);
    }
  });

  const article = new Readability(docClone).parse();
  const survivorIds = new Set();

  if (article && article.content) {
    const fragment =
        document.createRange().createContextualFragment(article.content);
    fragment.querySelectorAll('[data-readability-id]').forEach(
        element => survivorIds.add(element.dataset.readabilityId));
  }

  applyVisualizationStyles(elementMap, survivorIds);
}

/**
 * Applies CSS classes and styles to the document to visualize the results.
 * @param {Map<string, Element>} elementMap A map from ID to element in the
 *     visible DOM.
 * @param {Set<string>} survivorIds A set of IDs for elements that were kept.
 */
function applyVisualizationStyles(elementMap, survivorIds) {
  const styleId = 'readability-vis-style';
  if (!document.getElementById(styleId)) {
    const style = document.createElement('style');
    style.id = styleId;
    style.textContent = `
      .readability-kept {
        background-color: rgba(0, 255, 0, 0.2);
        outline: 1px solid rgba(0, 255, 0, 0.5);
      }
      .readability-discarded {
        background-color: rgba(255, 0, 0, 0.1);
        outline: 1px solid rgba(255, 0, 0, 0.3);
      }
    `;
    document.head.appendChild(style);
  }

  // Clear any previous visualization classes and styles.
  document.querySelectorAll('.readability-kept, .readability-discarded')
      .forEach(el => {
        el.classList.remove('readability-kept', 'readability-discarded');
        el.style.opacity = '';
        el.style.color = '';
      });

  // First pass: classify all elements as kept or discarded.
  for (const [id, element] of elementMap.entries()) {
    if (survivorIds.has(id)) {
      element.classList.add('readability-kept');
    } else {
      element.classList.add('readability-discarded');
    }
  }

  // Second pass: refine styling for "leaf" discarded nodes to avoid compounding
  // opacity issues.
  const discardedElements = document.querySelectorAll('.readability-discarded');
  const targetOpacity = 0.5;
  const fadedTextColor = 'rgba(0, 0, 0, 0.5)';

  discardedElements.forEach(element => {
    // A "leaf" is a discarded element that contains no "kept" descendants.
    if (!element.querySelector('.readability-kept')) {
      const tagName = element.tagName.toLowerCase();
      if (['img', 'video', 'picture', 'svg', 'canvas'].includes(tagName)) {
        // For media elements, reduce opacity directly.
        const currentOpacity =
            parseFloat(window.getComputedStyle(element).opacity);
        if (currentOpacity > targetOpacity) {
          element.style.opacity = targetOpacity;
        }
      } else {
        // For other elements, just fade the text color. This avoids compounding
        // opacity for nested containers.
        element.style.color = fadedTextColor;
      }
    }
  });
}

/******** Main Initialization ********/

(async () => {
  const data = await getClonedPageData();
  if (data) {
    renderClonedPage(data.html, data.styles);
  }
})();
