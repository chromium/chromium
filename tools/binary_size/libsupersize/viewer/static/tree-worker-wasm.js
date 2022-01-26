// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

importScripts('./auth-consts.js');
importScripts('./shared.js');
importScripts('./caspian_web.js');

const g_wasmPromise = new Promise(function(resolve, reject) {
  Module['onRuntimeInitialized'] = function() {
    console.log('Loaded WebAssembly runtime');
    resolve();
  }
});

const _PATH_SEP = '/';
const _NAMES_TO_FLAGS = Object.freeze({
  hot: _FLAGS.HOT,
  generated: _FLAGS.GENERATED_SOURCE,
  coverage: _FLAGS.COVERAGE,
  uncompressed: _FLAGS.UNCOMPRESSED,
});

let g_loadTreePromise = null;
let g_buildTreePromise = null;


/**
 * Wrapper around fetch for requesting the same resource multiple times.
 */
class DataFetcher {
  constructor(accessToken, url) {
    /** @type {string | null} */
    this._accessToken = accessToken;
    /** @type {string} */
    this._url = url;
  }

  _fetchFromGoogleCloudStorage() {
    const {bucket, file} = parseGoogleCloudStorageUrl(this._url);
    const params = `alt=media`;
    const api_url = `${STORAGE_API_ENDPOINT}/b/${bucket}/o/${file}?${params}`;
    const headers = new Headers();
    headers.append('Authorization', `Bearer ${this._accessToken}`);
    return this._fetchDirectly(api_url, headers);
  }

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

function looksLikeGoogleCloudStorage(url) {
  return url.startsWith('https://storage.googleapis.com/');
}

function parseGoogleCloudStorageUrl(url) {
  const re = /^https:\/\/storage\.googleapis\.com\/(?<bkt>[^\/]+)\/(?<file>.+)/;
  const match = re.exec(url);
  const bucket = encodeURIComponent(match.groups['bkt']);
  const file = encodeURIComponent(match.groups['file']);
  return {bucket, file};
}

function mallocBuffer(buf) {
  var dataPtr = Module._malloc(buf.byteLength);
  var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, buf.byteLength);
  dataHeap.set(new Uint8Array(buf));
  return dataHeap;
}

function sendProgressMessage(percent) {
  // @ts-ignore
  self.postMessage({percent, id: 0});
};

async function Open(name) {
  const wasmOpen = Module.cwrap('Open', 'number', ['string']);
  const stringPtr = wasmOpen(name);
  // Something has gone wrong if we get back a string longer than 67MB.
  return JSON.parse(Module.UTF8ToString(stringPtr, 2 ** 26));
}

function loadSizeFile(isBefore, sizeBuffer) {
  const heapBuffer = mallocBuffer(sizeBuffer);
  const wasmLoadSizeFile = Module.cwrap(
      isBefore ? 'LoadBeforeSizeFile' : 'LoadSizeFile', 'bool',
      ['number', 'number']);
  const start_time = Date.now();
  wasmLoadSizeFile(heapBuffer.byteOffset, sizeBuffer.byteLength);
  console.log(
      'Loaded size file in ' + (Date.now() - start_time) / 1000.0 + ' seconds');
  Module._free(heapBuffer.byteOffset);
}

function loadSizeProperties() {
  const wasmQueryProperty = Module.cwrap('QueryProperty', 'number', ['string']);
  const getProperty = (key) => {
    const stringPtr = wasmQueryProperty(key);
    const r = Module.UTF8ToString(stringPtr, 2 ** 16);
    return r;
  };
  return {
    isMultiContainer: (getProperty('isMultiContainer') === 'true')
  };
}

async function loadTree(input, accessToken, url, beforeUrl) {
  const isUpload = input !== 'from-url://';

  if (isUpload) {
    console.info('Displaying uploaded data');
  } else {
    console.info('Displaying data from', url);
  }
  const loadFetcher = new DataFetcher(accessToken, isUpload ? input : url);
  let beforeFetcher = null;
  if (beforeUrl) {
    beforeFetcher = new DataFetcher(accessToken, beforeUrl);
  }

  let isMultiContainer = null;
  let beforeBlobUrl = null;
  let loadBlobUrl = null;
  try {
    // It takes a few seconds to process large .size files, so download the main
    // file first, and then overlap its processing with the subsequent download.
    // Don't download both at the same time to ensure bandwidth is not split
    // between them.
    const mainSizeBuffer = await loadFetcher.fetchSizeBuffer();
    sendProgressMessage(.4);
    let beforeSizeBuffer = null;
    const beforeSizeBufferPromise = beforeFetcher?.fetchSizeBuffer();
    await loadSizeFile(false, mainSizeBuffer);
    sendProgressMessage(.6);
    if (beforeSizeBufferPromise) {
      beforeSizeBuffer = await beforeSizeBufferPromise;
      sendProgressMessage(.7);
      await loadSizeFile(true, beforeSizeBuffer);
    }
    sendProgressMessage(.8);
    const sizeProperties = await loadSizeProperties();
    isMultiContainer = sizeProperties.isMultiContainer;
    if (!isUpload) {
      loadBlobUrl = URL.createObjectURL(new Blob(
          [mainSizeBuffer.buffer], {type: 'application/octet-stream'}));
      if (beforeSizeBuffer) {
        beforeBlobUrl = URL.createObjectURL(new Blob(
            [beforeSizeBuffer.buffer], {type: 'application/octet-stream'}));
      }
    }
  } catch (e) {
    sendProgressMessage(1);
    throw e;
  }

  return {beforeBlobUrl, loadBlobUrl, isMultiContainer}
}

async function buildTree(
    groupBy, includeRegex, excludeRegex, includeSections, minSymbolSize,
    flagToFilter, methodCountMode) {
  const wasmBuildTree = Module.cwrap(
      'BuildTree', 'bool',
      ['bool', 'string', 'string', 'string', 'string', 'number', 'number']);
  const start_time = Date.now();
  const diffMode = wasmBuildTree(
      methodCountMode, groupBy, includeRegex, excludeRegex,
      includeSections, minSymbolSize, flagToFilter);
  console.log(
      'Constructed tree in ' + (Date.now() - start_time) / 1000.0 + ' seconds');
  return diffMode;
}

/**
 * Parse the options represented as a query string, into an object.
 * Includes checks for valid values.
 * @param {string} options Query string
 */
function parseOptions(options) {
  const params = new URLSearchParams(options);

  const groupBy = params.get('group_by') || 'source_path';
  const methodCountMode = params.has('method_count');

  const includeRegex = params.get('include');
  const excludeRegex = params.get('exclude');

  let includeSections = params.get('type');
  if (methodCountMode) {
    includeSections = _DEX_METHOD_SYMBOL_TYPE;
  } else if (includeSections === null) {
    // Exclude native symbols by default.
    const includeSectionsSet = new Set(_SYMBOL_TYPE_SET);
    includeSectionsSet.delete('b');
    includeSections = Array.from(includeSectionsSet.values()).join('');
  }

  const minSymbolSize = Number(params.get('min_size'));
  if (Number.isNaN(minSymbolSize)) {
    minSymbolSize = 0;
  }

  const flagToFilter = _NAMES_TO_FLAGS[params.get('flag_filter')] || 0;
  const url = params.get('load_url');
  const beforeUrl = params.get('before_url');

  return {
    groupBy,
    includeRegex,
    excludeRegex,
    includeSections,
    minSymbolSize,
    flagToFilter,
    methodCountMode,
    url,
    beforeUrl,
  };
}

const actions = {
  /**
   * @param {{input:string,accessToken:?string,options:string}} param0
   */
  async loadAndBuildTree({input, accessToken, options}) {
    const {
      url,
      beforeUrl,
    } = parseOptions(options);

    if (g_loadTreePromise) {
      // New loads should create new WebWorkers instead.
      throw new Error('loadTree with input called multiple times.');
    }
    g_loadTreePromise = loadTree(input, accessToken, url, beforeUrl);
    const loadResults = await g_loadTreePromise;
    const ret = await actions.buildTree({options});
    ret.loadResults = loadResults;
    return ret;
  },

  async buildTree({options}) {
    const {
      groupBy,
      includeRegex,
      excludeRegex,
      includeSections,
      minSymbolSize,
      flagToFilter,
      methodCountMode,
    } = parseOptions(options);

    // Ensure iniitial load is complete.
    await g_loadTreePromise;

    // Wait for queued up calls to complete. There should not be too many
    // since we debounce load calls.
    // TODO(huangs): Replace this and runActionDebounced() with explicit logic
    //     to cancel stale requests.
    while (g_buildTreePromise) {
      await g_buildTreePromise;
    }
    g_buildTreePromise = buildTree(
        groupBy, includeRegex, excludeRegex, includeSections, minSymbolSize,
        flagToFilter, methodCountMode);

    const diffMode = await g_buildTreePromise;
    g_buildTreePromise = null;
    sendProgressMessage(0.9);
    const root = await Open('');
    // TODO(crbug.com/1290946): Move diffMode to loadResults and do not store it
    //     the viewer's query parameters.
    return {
      root,
      diffMode,
    };
  },

  /** @param {string} path */
  async open(path) {
    return Open(path);
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
