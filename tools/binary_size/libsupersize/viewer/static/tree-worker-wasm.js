// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

importScripts('./auth-consts.js');
importScripts('./shared.js');
importScripts('./caspian_web.js');

/** @type {Promise} */
const g_wasmPromise = new Promise(function(resolve, reject) {
  Module['onRuntimeInitialized'] = function() {
    console.log('Loaded WebAssembly runtime');
    resolve();
  }
});

/** @type {string} */
const _PATH_SEP = '/';

/**
 * Limit SuperSize JSON size to 64 MiB; anything longer would be an anomaly.
 * @constant {number}
 */
const JSON_MAX_BYTES_TO_READ = 2 ** 26;

/** @type {?Promise} */
let g_loadTreePromise = null;

/** @type {?Promise} */
let g_buildTreePromise = null;


/**
 * Wrapper around fetch to request the same resource multiple times.
 */
class DataFetcher {
  constructor(accessToken, url) {
    /** @type {?string} */
    this._accessToken = accessToken;
    /** @type {string} */
    this._url = url;
  }

  /** @return {Promise<Response, Error>} */
  _fetchFromGoogleCloudStorage() {
    const {bucket, file} = parseGoogleCloudStorageUrl(this._url);
    const params = `alt=media`;
    const api_url = `${STORAGE_API_ENDPOINT}/b/${bucket}/o/${file}?${params}`;
    const headers = new Headers();
    headers.append('Authorization', `Bearer ${this._accessToken}`);
    return this._fetchDirectly(api_url, headers);
  }

  /**
   * @param {string} url
   * @param {?Headers=} headers
   * @return {Promise<Response, Error>}
   */
  async _fetchDirectly(url, headers = null) {
    if (!headers) {
      headers = new Headers();
    }
    const response = await fetch(url, {
      headers,
      credentials: 'same-origin',
    });
    if (!response.ok) {
      throw new Error('Fetch failed.');
    }
    return response;
  }

  /** @return {Promise<Uint8Array>} */
  async fetchSizeBuffer() {
    let response;
    if (this._accessToken && looksLikeGoogleCloudStorage(this._url)) {
      response = await this._fetchFromGoogleCloudStorage();
    } else {
      response = await this._fetchDirectly(this._url);
    }
    return new Uint8Array(await response.arrayBuffer());
  }
}

/** @param {string} url */
function looksLikeGoogleCloudStorage(url) {
  return url.startsWith('https://storage.googleapis.com/');
}

/** @param {string} url */
function parseGoogleCloudStorageUrl(url) {
  const re = /^https:\/\/storage\.googleapis\.com\/(?<bkt>[^\/]+)\/(?<file>.+)/;
  const match = re.exec(url);
  const bucket = encodeURIComponent(match.groups['bkt']);
  const file = encodeURIComponent(match.groups['file']);
  return {bucket, file};
}

/** @param {Uint8Array} buf */
function mallocBuffer(buf) {
  var dataPtr = Module._malloc(buf.byteLength);
  var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, buf.byteLength);
  dataHeap.set(new Uint8Array(buf));
  return dataHeap;
}

/** @param {number} percent */
function sendProgressMessage(percent) {
  // @ts-ignore
  self.postMessage({percent, id: 0});
};

/**
 * @param {boolean} isBefore
 * @param {Uint8Array} sizeBuffer
 */
function wasmLoadSizeFile(isBefore, sizeBuffer) {
  const heapBuffer = mallocBuffer(sizeBuffer);
  const cwrapLoadSizeFile = Module.cwrap(
      isBefore ? 'LoadBeforeSizeFile' : 'LoadSizeFile', 'bool',
      ['number', 'number']);
  const start_time = Date.now();
  cwrapLoadSizeFile(heapBuffer.byteOffset, sizeBuffer.byteLength);
  console.log(
      'Loaded size file in ' + (Date.now() - start_time) / 1000.0 + ' seconds');
  Module._free(heapBuffer.byteOffset);
}

/** @return {SizeProperties} */
function wasmLoadSizeProperties() {
  const cwrapQueryProperty =
      Module.cwrap('QueryProperty', 'number', ['string']);
  const getProperty = (key) => {
    const stringPtr = cwrapQueryProperty(key);
    const r = Module.UTF8ToString(stringPtr, 2 ** 16);
    return r;
  };
  return {
    isMultiContainer: (getProperty('isMultiContainer') === 'true')
  };
}

/**
 * @param {number} id
 * @return {QueryAncestryResults}
 */
function wasmQueryAncestryById(id) {
  const cwrapQueryAncestryById =
      Module.cwrap('QueryAncestryById', 'number', ['number']);
  const stringPtr = cwrapQueryAncestryById(id);
  const r = Module.UTF8ToString(stringPtr, 2 ** 16);
  return /** @type {!QueryAncestryResults} */ (JSON.parse(r));
}

/**
 * @param {string} input
 * @param {?string} accessToken
 * @param {!BuildOptions} buildOptions
 * @return {Promise<!LoadTreeResults, Error>}
 */
async function loadTreeWorkhorse(input, accessToken, buildOptions) {
  const {
    loadUrl,
    beforeUrl,
  } = buildOptions;

  const isUpload = (input !== 'from-url://');
  if (isUpload) {
    console.info('Displaying uploaded data');
  } else {
    console.info('Displaying data from', loadUrl);
  }
  const loadFetcher = new DataFetcher(accessToken, isUpload ? input : loadUrl);
  let beforeFetcher = null;
  if (beforeUrl) {
    beforeFetcher = new DataFetcher(accessToken, beforeUrl);
  }

  let isMultiContainer = null;
  let beforeBlobUrl = null;
  let loadBlobUrl = null;
  let metadata = null;
  try {
    // It takes a few seconds to process large .size files, so download the main
    // file first, and then overlap its processing with the subsequent download.
    // Don't download both at the same time to ensure bandwidth is not split
    // between them.
    const mainSizeBuffer = await loadFetcher.fetchSizeBuffer();
    sendProgressMessage(.4);
    let beforeSizeBuffer = null;
    const beforeSizeBufferPromise = beforeFetcher?.fetchSizeBuffer();
    wasmLoadSizeFile(false, mainSizeBuffer);
    sendProgressMessage(.6);
    if (beforeSizeBufferPromise) {
      beforeSizeBuffer = await beforeSizeBufferPromise;
      sendProgressMessage(.7);
      wasmLoadSizeFile(true, beforeSizeBuffer);
    }
    sendProgressMessage(.8);
    const sizeProperties = wasmLoadSizeProperties();
    isMultiContainer = sizeProperties.isMultiContainer;
    if (!isUpload) {
      // Revoked in displayOrHideDownloadButton().
      loadBlobUrl = URL.createObjectURL(new Blob(
          [mainSizeBuffer.buffer], {type: 'application/octet-stream'}));
      if (beforeSizeBuffer) {
        // Revoked in displayOrHideDownloadButton().
        beforeBlobUrl = URL.createObjectURL(new Blob(
            [beforeSizeBuffer.buffer], {type: 'application/octet-stream'}));
      }
    }
    metadata = wasmLoadMetadata();
  } catch (e) {
    sendProgressMessage(1);
    throw e;
  }

  return {isMultiContainer, beforeBlobUrl, loadBlobUrl, metadata};
}

/**
 * @return {MetadataType}
 */
function wasmLoadMetadata() {
  const cwrapGetMetaData = Module.cwrap('GetMetadata', 'number', ['']);
  const stringPtr = cwrapGetMetaData();
  return /** @type {MetadataType} */ (
      JSON.parse(Module.UTF8ToString(stringPtr, JSON_MAX_BYTES_TO_READ)));
}

/**
 * @param {!BuildOptions} buildOptions
 * @return {Promise<boolean>}
 */
async function wasmBuildTree(buildOptions) {
  const {
    methodCountMode,
    groupBy,
    includeRegex,
    excludeRegex,
    includeSections,
    minSymbolSize,
    flagToFilter,
    nonOverhead,
    disassemblyMode,
  } = buildOptions;

  const cwrapBuildTree = Module.cwrap(
      'BuildTree', 'bool',
      ['bool', 'string', 'string', 'string', 'string', 'number', 'number']);
  const start_time = Date.now();
  const diffMode = cwrapBuildTree(
      methodCountMode, groupBy, includeRegex, excludeRegex, includeSections,
      minSymbolSize, flagToFilter, nonOverhead, disassemblyMode);
  console.log(
      'Constructed tree in ' + (Date.now() - start_time) / 1000.0 + ' seconds');
  return diffMode;
}

/**
 * @param {string} name
 * @return {Promise<TreeNode>}
 */
async function wasmOpen(name) {
  const cwrapOpen = Module.cwrap('Open', 'number', ['string']);
  const stringPtr = cwrapOpen(name);
  return /** @type {TreeNode} */ (
      JSON.parse(Module.UTF8ToString(stringPtr, JSON_MAX_BYTES_TO_READ)));
}

/**
 * The functions in `action` are referenced to by TreeWorker as strings, and
 * destructured object parameters are specified by keys. When remaining these,
 * be sure to update these strings / keys.
 */
const actions = {
  /**
   * @param {{input:string,accessToken:?string,buildOptions:BuildOptions}}
   *     param0
   * @return {Promise<BuildTreeResults, Error>}
   */
  async loadAndBuildTree({input, accessToken, buildOptions}) {
    if (g_loadTreePromise) {
      // New loads should create new WebWorkers instead.
      throw new Error('loadTree with input called multiple times.');
    }
    g_loadTreePromise = loadTreeWorkhorse(input, accessToken, buildOptions);
    const loadResults = await g_loadTreePromise;
    const ret = await actions.buildTree({buildOptions});
    ret.loadResults = loadResults;
    return ret;
  },

  /**
   * @param {{buildOptions:BuildOptions}} param0
   * @return {Promise<BuildTreeResults>}
   */
  async buildTree({buildOptions}) {
    // Ensure initial load is complete.
    await g_loadTreePromise;

    // Wait for queued up calls to complete. There should not be too many
    // since we debounce load calls.
    // TODO(huangs): Replace this and runActionDebounced() with explicit logic
    //     to cancel stale requests.
    while (g_buildTreePromise) {
      await g_buildTreePromise;
    }
    g_buildTreePromise = wasmBuildTree(buildOptions);

    const diffMode = await g_buildTreePromise;
    g_buildTreePromise = null;
    sendProgressMessage(0.9);
    const root = await wasmOpen('');
    // TODO(crbug.com/40818460): Move diffMode to loadResults and do not store
    // it the viewer's query parameters.
    return {
      root,
      diffMode,
      loadResults: null,
    };
  },

  /**
   * @param {string} path
   * @return {Promise<TreeNode>}
   */
  async open(path) {
    return wasmOpen(path);
  },

  /**
   * @param {number} id
   * @return {Promise<QueryAncestryResults>}
   */
  async queryAncestryById(id) {
    return wasmQueryAncestryById(id);
  },
};

/**
 * Call the requested action function with the given data. If an error is thrown
 * or rejected, post the error message to the UI thread.
 * @param {number} id Unique message ID.
 * @param {string} action Action type, corresponding to a key in `actions.`
 * @param {any} data Data to supply to the action function.
 */
async function runAction(id, action, data) {
  try {
    const result = await actions[action](data);
    // @ts-ignore
    self.postMessage({id, result});
  } catch (err) {
    // @ts-ignore
    self.postMessage({id: 0, error: err.message});
    throw err;
  }
}

const runActionDebounced = debounce(runAction, 0);

/**
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = async event => {
  await g_wasmPromise;
  const {id, action, data} = event.data;
  if (action === 'buildTree') {
    // Loading large files will block the worker thread until complete or when
    // an await statement is reached. During this time, multiple load messages
    // can pile up due to filters being adjusted. We debounce the load call
    // so that only the last message is read (the current set of filters).
    runActionDebounced(id, action, data);
  } else {
    runAction(id, action, data);
  }
};
