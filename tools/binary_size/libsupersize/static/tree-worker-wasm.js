// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

importScripts('./auth-consts.js');
importScripts('./shared.js');
importScripts('./caspian_web.js');

const LoadWasm = new Promise(function(resolve, reject) {
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


/**
 * Wrapper around fetch for requesting the same resource multiple times.
 */
class DataFetcher {
  constructor(accessToken) {
    /** @type {string | null} */
    this._accessToken = accessToken;
    /** @type {AbortController | null} */
    this._controller = null;
    /** @type {string | null} */
    this._input = null;
    /** @type {Uint8Array | null} */
    this._cache = null;
  }

  /**
   * Sets the input that describes what will be fetched. Also clears the cache.
   * @param {string | Request} input URL to the resource you want to fetch.
   */
  setInput(input) {
    if (this._input && this._input.startsWith('blob:')) {
      // Revoke the previous Blob url to prevent memory leaks
      URL.revokeObjectURL(this._input);
    }

    this._cache = null;
    this._input = input;
  }

  /**
   * Starts a new request and aborts the previous one.
   * @param {string | Request} url
   */
  async fetchUrl(url) {
    if (this._accessToken && looksLikeGoogleCloudStorage(url)) {
      return this._fetchFromGoogleCloudStorage(url);
    } else {
      return this._doFetch(url);
    }
  }

  async _fetchFromGoogleCloudStorage(url) {
    const {bucket, file} = parseGoogleCloudStorageUrl(url);
    const params = `alt=media`;
    const api_url = `${STORAGE_API_ENDPOINT}/b/${bucket}/o/${file}?${params}`;
    const headers = new Headers();
    headers.append('Authorization', `Bearer ${this._accessToken}`);
    return this._doFetch(api_url, headers);
  }

  async _doFetch(url, headers) {
    if (this._controller) this._controller.abort();
    this._controller = new AbortController();
    if (!headers) {
      headers = new Headers();
    }
    let response = await fetch(url, {
      headers,
      credentials: 'same-origin',
      signal: this._controller.signal,
    });
    if (!response.ok) {
      throw new Error('Fetch failed.');
    }
    return response;
  }

  /**
   * Outputs a single UInt8Array encompassing the entire input .size file.
   */
  async loadSizeBuffer() {
    if (!this._cache) {
      const response = await this.fetchUrl(this._input);
      this._cache = new Uint8Array(await response.arrayBuffer());
    }
    return this._cache;
  }
}

function looksLikeGoogleCloudStorage(url) {
  return url.startsWith('https://storage.googleapis.com/');
}

function parseGoogleCloudStorageUrl(url) {
  const re = /^https:\/\/storage\.googleapis\.com\/(?<bucket>[^\/]+)\/(?<file>.+)/;
  const match = re.exec(url);
  const bucket = encodeURIComponent(match.groups['bucket']);
  const file = encodeURIComponent(match.groups['file']);
  return {bucket, file};
}

function mallocBuffer(buf) {
  var dataPtr = Module._malloc(buf.byteLength);
  var dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, buf.byteLength);
  dataHeap.set(new Uint8Array(buf));
  return dataHeap;
}

async function Open(name) {
  return LoadWasm.then(() => {
    _Open = Module.cwrap('Open', 'number', ['string']);
    const stringPtr = _Open(name);
    // Something has gone wrong if we get back a string longer than 67MB.
    const ret = JSON.parse(Module.UTF8ToString(stringPtr, 2 ** 26));
    return ret;
  });
}

let g_fetcher = null;
let g_beforeFetcher = null;
let g_sizeFileLoaded = false;

/** @type {SizeProperties} */
let g_size_properties = null;

async function loadSizeFile(isBefore, fetcher) {
  const sizeBuffer = await fetcher.loadSizeBuffer();
  const heapBuffer = mallocBuffer(sizeBuffer);
  const LoadSizeFile = Module.cwrap(
      isBefore ? 'LoadBeforeSizeFile' : 'LoadSizeFile', 'bool',
      ['number', 'number']);
  const start_time = Date.now();
  LoadSizeFile(heapBuffer.byteOffset, sizeBuffer.byteLength);
  console.log(
      'Loaded size file in ' + (Date.now() - start_time) / 1000.0 + ' seconds');
  Module._free(heapBuffer.byteOffset);
}

async function loadSizeProperties() {
  const QueryProperty = Module.cwrap('QueryProperty', 'number', ['string']);
  const getProperty = (key) => {
    const stringPtr = QueryProperty(key);
    const r = Module.UTF8ToString(stringPtr, 2 ** 16);
    return r;
  };
  g_size_properties = {
    isMultiContainer: (getProperty('isMultiContainer') === 'true')
  };
}

async function buildTree(
    groupBy, includeRegex, excludeRegex, includeSections, minSymbolSize,
    flagToFilter, methodCountMode, onProgress) {

  onProgress({percent: 0.1, id: 0});
  /** @type {Metadata} */
  return await LoadWasm.then(async () => {
    if (!g_sizeFileLoaded) {
      const load_promises = [];
      load_promises.push(loadSizeFile(false, g_fetcher));
      if (g_beforeFetcher !== null) {
        load_promises.push(loadSizeFile(true, g_beforeFetcher));
      }
      try {
        await Promise.all(load_promises).then(loadSizeProperties);
      } catch (e) {
        onProgress({percent: 1, id: 0});
        throw e;
      }
      onProgress({percent: 0.4, id: 0});
      g_sizeFileLoaded = true;
    }

    const BuildTree = Module.cwrap(
        'BuildTree', 'bool',
        ['bool', 'string', 'string', 'string', 'string', 'number', 'number']);
    const start_time = Date.now();
    const diffMode = BuildTree(
        methodCountMode, groupBy, includeRegex, excludeRegex,
        includeSections, minSymbolSize, flagToFilter);
    console.log(
        'Constructed tree in ' + (Date.now() - start_time) / 1000.0 +
        ' seconds');
    onProgress({percent: 0.8, id: 0});

    const root = await Open('');
    return {
      root,
      percent: 1.0,
      diffMode,
      isMultiContainer: g_size_properties.isMultiContainer,
    };
  });
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
    let includeSectionsSet = new Set(_SYMBOL_TYPE_SET);
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
  /** @param {{input:string|null,accessToken:string|null,options:string}} param0 */
  load({input, accessToken, options}) {
    const {
      groupBy,
      includeRegex,
      excludeRegex,
      includeSections,
      minSymbolSize,
      flagToFilter,
      methodCountMode,
      url,
      beforeUrl,
    } = parseOptions(options);
    if (!g_fetcher) {
      g_fetcher = new DataFetcher(accessToken);
    }
    if (input === 'from-url://' && url) {
      // Display the data from the `load_url` query parameter
      console.info('Displaying data from', url);
      g_fetcher.setInput(url);
    } else if (input != null) {
      console.info('Displaying uploaded data');
      g_fetcher.setInput(input);
    }

    if (beforeUrl) {
      g_beforeFetcher = new DataFetcher(accessToken);
      g_beforeFetcher.setInput(beforeUrl);
    }

    return buildTree(
        groupBy, includeRegex, excludeRegex, includeSections, minSymbolSize,
        flagToFilter, methodCountMode, progress => {
          // @ts-ignore
          self.postMessage(progress);
        });
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
    self.postMessage({id, error: err.message});
    throw err;
  }
}

const runActionDebounced = debounce(runAction, 0);

/**
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = async event => {
  const {id, action, data} = event.data;
  if (action === 'load') {
    // Loading large files will block the worker thread until complete or when
    // an await statement is reached. During this time, multiple load messages
    // can pile up due to filters being adjusted. We debounce the load call
    // so that only the last message is read (the current set of filters).
    runActionDebounced(id, action, data);
  } else {
    runAction(id, action, data);
  }
};
