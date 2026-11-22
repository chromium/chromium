// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI glue code and instances (including singletons).
 */

(function() {
  /** @type {ProgressBar} */
  const _progress = new ProgressBar(g_el.progAppbar);

  /** @type {!SymbolTreeUi} */
  const _symbolTreeUi = new SymbolTreeUi();
  _symbolTreeUi.init();

  /** @type {!MetricsTreeModel} */
  const _metricsTreeModel = new MetricsTreeModel();

  /** @type {!MetricsTreeUi} */
  const _metricsTreeUi = new MetricsTreeUi(_metricsTreeModel);
  _metricsTreeUi.init();

  /** @type {!MetadataTreeModel} */
  const _metadataTreeModel = new MetadataTreeModel();

  /** @type {!MetadataTreeUi} */
  const _metadataTreeUi = new MetadataTreeUi(_metadataTreeModel);
  _metadataTreeUi.init();

  /**
   * Copy small CSV exports directly to the clipboard; download larger ones as
   * a file to avoid slow clipboard writes.
   */
  const _CSV_CLIPBOARD_MAX_SYMBOLS = 1000;
  const _CSV_COPIED_FEEDBACK_MS = 1500;

  /** @type {number} */
  let _filteredSymbolCount = 0;

  /** @type {number} */
  let _copyResetTimerId = 0;

  /** @type {number} */
  let _rebuildSeq = 0;

  /** @type {number} */
  let _completedRebuildSeq = 0;

  /** @type {boolean} */
  let _csvExportInFlight = false;

  /**
   * @param {number} symbolCount
   * @return {boolean}
   */
  function canCopyCsvToClipboard(symbolCount) {
    return symbolCount < _CSV_CLIPBOARD_MAX_SYMBOLS &&
        Boolean(navigator.clipboard);
  }

  function updateCsvButtonDisabled() {
    g_el.btnDownloadCsv.disabled =
        _completedRebuildSeq !== _rebuildSeq || _filteredSymbolCount === 0;
  }

  function updateCsvButtonTitle() {
    clearTimeout(_copyResetTimerId);
    g_el.btnDownloadCsv.classList.remove('copied');
    g_el.spanLiveStatus.textContent = '';
    const actionLabel =
        canCopyCsvToClipboard(_filteredSymbolCount) ? 'Copy' : 'Download';
    const symbolNoun = _filteredSymbolCount === 1 ? 'symbol' : 'symbols';
    const label = `${actionLabel} ${formatNumber(_filteredSymbolCount)} ${
        symbolNoun} as CSV`;
    g_el.btnDownloadCsv.title = label;
    g_el.btnDownloadCsv.setAttribute('aria-label', label);
  }

  /** @param {TreeProgress} message */
  function onProgressMessage(message) {
    const {error, percent} = message;
    _progress.setValue(percent);
    document.body.classList.toggle('error', Boolean(error));
    if (error) {
      // Worker errors post to id: 0 without settling request promises.
      _completedRebuildSeq = _rebuildSeq;
      _csvExportInFlight = false;
      updateCsvButtonDisabled();
    }
  }

  /**
   * Processes response of an initial load / upload.
   * @param {BuildTreeResults} message
   */
  function processLoadTreeResponse(message) {
    const {diffMode} = message;
    const {beforeBlobUrl, loadBlobUrl, isMultiContainer, metadata} =
        message.loadResults;
    console.log(
        '%cPro Tip: %cawait supersize.worker.openNode("$FILE_PATH")',
        'font-weight:bold;color:red;', '')

    window.supersize.metadata = metadata;
    for (const key of ['size_file', 'before_size_file']) {
      if (metadata.hasOwnProperty(key))
        preprocessSizeFileInPlace(metadata[key]);
    }

    displayOrHideDownloadButton(beforeBlobUrl, loadBlobUrl);

    state.setDiffMode(diffMode);
    document.body.classList.toggle('diff', Boolean(diffMode));

    processBuildTreeResponse(message);
    renderAndShowMetricsTree(metadata);
    renderAndShowMetadataTree(metadata);
    setReviewInfo(metadata);
  }

  /**
   * Sets the review URL and title from message to the HTML element.
   * @param {MetadataType} metadata
   */
  function setReviewInfo(metadata) {
    const processReviewInfo = (field) => {
      const urlExists = Boolean(
          field?.hasOwnProperty('url') && field?.hasOwnProperty('title'));
      if (urlExists) {
        g_el.linkReviewText.href = field['url'];
        g_el.linkReviewText.textContent = field['title'];
      }
      g_el.divReviewInfo.style.display = urlExists ? '' : 'none';
    };
    const sizeFile = metadata['size_file'];
    if (sizeFile?.hasOwnProperty('build_config')) {
      processReviewInfo(sizeFile['build_config'])
    }
  }

  /**
   * Processes the result of a buildTree() message.
   * @param {BuildTreeResults} message
   */
  function processBuildTreeResponse(message) {
    const {root} = message;
    _progress.setValue(1);

    const noSymbols = (Object.keys(root.childStats).length === 0);
    _symbolTreeUi.toggleNoSymbolsMessage(noSymbols);

    _filteredSymbolCount = /** @type {!TreeNode} */ (root).symbolCount ?? 0;
    updateCsvButtonTitle();
    updateCsvButtonDisabled();
    g_el.btnDownloadCsv.hidden = false;

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (root) {
      rootElement = _symbolTreeUi.makeNodeElement(root);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node.
      link.click();
      link.tabIndex = 0;
    }

    // Double requestAnimationFrame ensures that the code inside executes in a
    // different frame than the above tree element creation.
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        dom.replace(g_el.ulSymbolTree, rootElement);
      });
    });
  }

  /**
   * Displays/hides download buttons for loadUrl.size and beforeUrl.size.
   * @param {?string=} beforeUrl
   * @param {?string=} loadUrl
   */
  function displayOrHideDownloadButton(beforeUrl = null, loadUrl = null) {
    const updateAnchor = (anchor, url) => {
      anchor.style.display = url ? '' : 'none';
      if (anchor.href && anchor.href.startsWith('blob:')) {
        URL.revokeObjectURL(anchor.href);
      }
      anchor.href = url;
    };
    updateAnchor(g_el.linkDownloadBefore, beforeUrl);
    updateAnchor(g_el.linkDownloadLoad, loadUrl);

    if (/** @type {string} */ (state.stLoadUrl.get()).includes('.sizediff')) {
      g_el.linkDownloadLoad.title = 'Download .sizediff file';
      g_el.linkDownloadLoad.download = 'load_size.sizediff';
    }
  }

  /**
   * Extracts metrics data and renders the Metrics Tree.
   * @param {?Object} metadata Used to compute Metrics Tree data. If null, reuse
   *     cached data. Otherwise new data is saved to cache.
   */
  function renderAndShowMetricsTree(metadata) {
    _metricsTreeModel.updateFilter();
    _metricsTreeModel.extractAndStoreRoot(metadata);

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (_metricsTreeModel.rootNode) {
      rootElement = _metricsTreeUi.makeNodeElement(_metricsTreeModel.rootNode);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node.
      link.click();
      link.tabIndex = 0;
    }

    dom.replace(g_el.ulMetricsTree, rootElement);
    g_el.divMetricsView.classList.toggle('active', true);
  }

  /**
   * Modifies per-container metadata in-place so they render better.
   * @param {Object} subMetadata
   */
  function formatMetadataInPlace(subMetadata) {
    if (subMetadata?.hasOwnProperty('elf_mtime')) {
      const date = new Date(subMetadata['elf_mtime'] * 1000);
      subMetadata['elf_mtime'] = date.toString();
    }
  }

  /**
   * Modifies size_file / before_size_file in-place so they render better.
   * @param {MetadataType} sizeFile
   */
  function preprocessSizeFileInPlace(sizeFile) {
    const processContainer = (container) => {
      if (container?.hasOwnProperty('metadata')) {
        formatMetadataInPlace(container['metadata']);
      }
      // Strip section_sizes because it is already shown in tree.
      if (container?.hasOwnProperty('section_sizes')) {
        delete container['section_sizes'];
      }
    };
    if (sizeFile?.hasOwnProperty('containers')) {
      for (const container of sizeFile['containers']) {
        processContainer(container);
      }
    } else {
      // Covers the case if the metadata is in old schema.
      processContainer(sizeFile);
    }
  }

  /**
   * Renders the Metadata Tree.
   * @param {?Object} metadata
   */
  function renderAndShowMetadataTree(metadata) {
    _metadataTreeModel.extractAndStoreRoot(metadata);
    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (_metricsTreeModel.rootNode) {
      rootElement =
          _metadataTreeUi.makeNodeElement(_metadataTreeModel.rootNode);
    }

    dom.replace(g_el.ulMetadataTree, rootElement);
    g_el.divMetadataView.classList.toggle('active', true);
  }

  /**
   * @param {!TreeWorker} worker
   * @return {!Promise}
   */
  async function planSymbolTreeFocusPathExpansionIfRequired(worker) {
    const focusStr = state.stFocus.get();
    if (focusStr) {
      const focus = parseInt(focusStr, 10);
      if (!isNaN(focus)) {
        const ancestryResults = await worker.queryAncestryById(focus);
        if (ancestryResults.ancestorIds?.length) {
          _symbolTreeUi.planPathExpansion(ancestryResults.ancestorIds);
          _symbolTreeUi.focus();
          return;
        }
      }
      state.stFocus.set('');  // Clear invalid value.
    }
  }

  /** @param {!Array<!URL>} urlsToLoad */
  async function performInitialLoad(urlsToLoad) {
    let accessToken = null;
    _progress.setValue(0.1);
    if (requiresAuthentication(urlsToLoad)) {
      accessToken = await fetchAccessToken();
      _progress.setValue(0.2);
    }
    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    let message;
    try {
      message = await worker.loadAndBuildTree('from-url://', accessToken);
    } catch (err) {
      // The token may have been revoked. Prompt for a new one next time.
      if (accessToken) {
        clearCachedToken();
      }
      throw err;
    }
    await planSymbolTreeFocusPathExpansionIfRequired(worker);
    processLoadTreeResponse(message);
  }

  async function rebuildTree() {
    _progress.setValue(0);
    const seq = ++_rebuildSeq;
    updateCsvButtonDisabled();
    const message = await window.supersize.worker.buildTree();
    if (seq === _rebuildSeq) {
      _completedRebuildSeq = seq;
    }
    processBuildTreeResponse(message);
    renderAndShowMetricsTree(null);
  }

  g_el.fileUpload.addEventListener('change', async (event) => {
    _progress.setValue(0.1);
    const input = /** @type {HTMLInputElement} */ (event.currentTarget);
    const file = input.files.item(0);
    const fileUrl = URL.createObjectURL(file);

    state.stLoadUrl.set(fileUrl);

    g_el.btnDownloadCsv.hidden = true;
    _filteredSymbolCount = 0;
    updateCsvButtonTitle();
    _csvExportInFlight = false;
    const seq = ++_rebuildSeq;
    updateCsvButtonDisabled();

    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    const message = await worker.loadAndBuildTree(fileUrl);
    URL.revokeObjectURL(fileUrl);
    if (worker !== window.supersize.worker) {
      return;
    }
    if (seq === _rebuildSeq) {
      _completedRebuildSeq = seq;
    }
    processLoadTreeResponse(message);
    // Clean up afterwards so new files trigger event.
    input.value = '';
  });

  g_el.btnDownloadCsv.addEventListener('click', async () => {
    if (_csvExportInFlight) {
      return;
    }
    _csvExportInFlight = true;
    const worker = window.supersize.worker;
    let csv = '';
    let symbolCount = 0;
    try {
      ({csv, symbolCount} = await worker.dumpCsv());
    } finally {
      if (worker === window.supersize.worker) {
        _csvExportInFlight = false;
      }
    }
    if (worker !== window.supersize.worker || !csv || symbolCount === 0) {
      return;
    }
    if (canCopyCsvToClipboard(symbolCount)) {
      try {
        await navigator.clipboard.writeText(csv);
        if (worker !== window.supersize.worker) {
          return;
        }
        clearTimeout(_copyResetTimerId);
        g_el.btnDownloadCsv.classList.add('copied');
        g_el.btnDownloadCsv.title = 'Copied CSV to clipboard';
        g_el.spanLiveStatus.textContent = 'Copied CSV to clipboard';
        _copyResetTimerId =
            setTimeout(updateCsvButtonTitle, _CSV_COPIED_FEEDBACK_MS);
        return;
      } catch (err) {
        // Fall back to downloading if clipboard write is blocked.
      }
    }
    if (worker !== window.supersize.worker) {
      return;
    }
    const blob = new Blob([csv], {type: 'text/csv;charset=utf-8'});
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = 'symbols.csv';
    anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 0);
  });

  g_el.frmOptions.addEventListener('change', (event) => {
    // Update the tree when options change.
    // Some options update the tree themselves, don't regenerate when those
    // options (marked by "data-dynamic") are changed.
    if (!/** @type {HTMLElement} */ (event.target)
             .dataset.hasOwnProperty('dynamic')) {
      rebuildTree();
    }
  });
  g_el.frmOptions.addEventListener('submit', (event) => {
    event.preventDefault();
    rebuildTree();
  });

  const urlsToLoad = [];
  for (const url of [state.stBeforeUrl.get(), state.stLoadUrl.get()]) {
    if (url)
      urlsToLoad.push(new URL(/** @type {string} */ (url), document.baseURI));
  }
  if (urlsToLoad.length > 0)
    performInitialLoad(urlsToLoad);

  // By default, updating the hash portion of the URL via UI does not cause page
  // refresh. We can intercept this for interesting navigation feature, but for
  // now just refresh the page to be consistent with other changes to the URL.
  window.addEventListener('hashchange', (event) => {
    window.location.reload();
  });
})();
