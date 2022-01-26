// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

window.supersize = window.supersize || {};
window.supersize.worker = null;

/**
 * We use a worker to keep large tree creation logic off the UI thread.
 * This class is used to interact with the worker.
 */
class TreeWorker {
  /**
   * @param {Worker} worker Web worker to wrap
   */
  constructor(worker, onProgressHandler) {
    this._worker = worker;
    /** ID counter used by `waitForResponse` */
    this._requestId = 1;

    this._worker.addEventListener('message', event => {
      // An ID of 0 means it's a progress event.
      if (event.data.id === 0) {
        onProgressHandler(event.data);
      }
    });
  }

  /**
   *
   * @param {string} action
   * @param {any} data
   * @returns {any}
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
   * Loads a new file.
   * @param {?string=} input
   * @param {?string=} accessToken
   * @returns {Promise<any>}
   */
  loadAndBuildTree(input = null, accessToken = null) {
    return this._waitForResponse('loadAndBuildTree', {
      input,
      accessToken,
      options: location.search.slice(1),
    });
  }

  /**
   * Rebuilds the tree with the current query parameters.
   * @returns {Promise<any>}
   */
  buildTree() {
    return this._waitForResponse('buildTree', {
      options: location.search.slice(1),
    });
  }
}

function restartWorker(onProgressHandler) {
  const innerWorker = new Worker('tree-worker-wasm.js');
  window.supersize.worker = new TreeWorker(innerWorker, onProgressHandler);
  return window.supersize.worker;
}
