// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * We use a worker to keep large tree creation logic off the UI thread.
 * This class is used to interact with the worker.
 */
class TreeWorker {
  /**
   * @param {Worker} worker Web worker to wrap
   */
  constructor(worker) {
    this._worker = worker;
    /** ID counter used by `waitForResponse` */
    this._requestId = 1;

    /** @type {(data: TreeProgress) => void | null} callback for `loadTree` */
    this._loadTreeCallback = null;

    this._worker.addEventListener('message', event => {
      if (this._loadTreeCallback && event.data.id === 0) {
        this._loadTreeCallback(event.data);
      }
    });
  }

  /**
   *
   * @param {string} action
   * @param {any} data
   */
  _waitForResponse(action, data) {
    const id = ++this._requestId;
    return new Promise((resolve, reject) => {
      const handleResponse = event => {
        if (event.data.id === id) {
          this._worker.removeEventListener('message', handleResponse);
          if (event.data.error) {
            reject(event.data.error);
          } else {
            resolve(event.data.result);
          }
        }
      };

      this._worker.addEventListener('message', handleResponse);
      this._worker.postMessage({id, action, data});
    });
  }

  /**
   * Get data for a node with `idPath`. Loads information about the node and its
   * direct children. Deeper children can be loaded by calling this function
   * again.
   * @param {string} idPath Path of the node to find
   * @returns {Promise<TreeNode | null>}
   */
  openNode(idPath) {
    return this._waitForResponse('open', idPath);
  }

  /**
   * Set callback used after `loadTree` is first called.
   * @param {(data: TreeProgress) => void} callback Called when the worker
   * has some data to display. Complete when `progress` is 1.
   */
  setOnProgressHandler(callback) {
    this._loadTreeCallback = callback;
  }

  /**
   * Loads the tree data given on a worker thread and replaces the tree view in
   * the UI once complete. Uses query string as state for the options.
   * Use `onProgress` before calling `loadTree`.
   * @param {?string=} input
   * @param {?string=} accessToken
   * @returns {Promise<TreeProgress>}
   */
  loadTree(input = null, accessToken = null) {
    return this._waitForResponse('load', {
      input,
      accessToken,
      options: location.search.slice(1),
    });
  }
}

window.supersize = {
  worker: null,
  treeReady: null,
};

// .size files and .ndjson files require different web workers.
// Switch between the two dynamically.
function startWorkerForFileName(fileName) {
  let innerWorker = null;
  if (fileName &&
      (fileName.endsWith('.size') || fileName.endsWith('.sizediff'))) {
    console.log('Using WebAssembly web worker');
    innerWorker = new Worker('tree-worker-wasm.js');
  } else {
    console.log('Using JavaScript web worker');
    innerWorker = new Worker('tree-worker.js');
  }
  window.supersize.worker = new TreeWorker(innerWorker);
}

(function() {
  const urlParams = new URLSearchParams(window.location.search);
  const url = urlParams.get('load_url');
  startWorkerForFileName(url);

  if (requiresAuthentication()) {
    window.supersize.treeReady = window.googleAuthPromise.then((authResponse) =>
        window.supersize.worker.loadTree('from-url://',
          authResponse.access_token));
  } else {
    window.supersize.treeReady = window.supersize.worker.loadTree('from-url://');
  }

})()
