// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * We use a worker to keep large tree creation logic off the UI thread.
 * This class is used to interact with the worker.
 */
class TreeWorker {
  /**
   * @param {Worker} worker Web worker to wrap
   * @param {function(TreeProgress): *} onProgressHandler
   */
  constructor(worker, onProgressHandler) {
    /** @const {Worker} */
    this._worker = worker;

    /** @type {number} ID counter used by `waitForResponse` */
    this._requestId = 1;

    this._worker.addEventListener('message', event => {
      // An ID of 0 means it's a progress event.
      if (event.data.id === 0) {
        onProgressHandler(event.data);
      }
    });
  }

  /** @public **/
  dispose() {
    this._worker.terminate();
    delete this._worker;
  }

  /**
   * @param {string} action
   * @param {any} data
   * @returns {Promise<*, string>}
   * @private
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
   * Loads a new file.
   * @param {?string=} input
   * @param {?string=} accessToken
   * @returns {Promise<BuildTreeResults, string>}
   */
  loadAndBuildTree(input=null, accessToken=null) {
    const buildOptions = state.exportToBuildOptions();
    return this._waitForResponse('loadAndBuildTree', {
      input,
      accessToken,
      buildOptions,
    });
  }

  /**
   * Rebuilds the tree with the current query parameters.
   * @returns {Promise<BuildTreeResults>}
   */
  buildTree() {
    state.stFocus.set('');
    const buildOptions = state.exportToBuildOptions();
    return this._waitForResponse('buildTree', {
      buildOptions,
    });
  }

  /**
   * Get data for a node with `idPath`. Loads information about the node and its
   * direct children. Deeper children can be loaded by calling this function
   * again.
   * @param {string} idPath Path of the node to find
   * @returns {Promise<TreeNode, string>}
   */
  openNode(idPath) {
    return this._waitForResponse('open', idPath);
  }

  /**
   * @param {number} id
   * @return {Promise<QueryAncestryResults>}
   */
  queryAncestryById(id) {
    return this._waitForResponse('queryAncestryById', id);
  }
}
/**
 * @param {function(TreeProgress): *} onProgressHandler
 * @return {TreeWorker}
 */
function restartWorker(onProgressHandler) {
  if (window.supersize.worker) {
    window.supersize.worker.dispose();
    window.supersize.worker = null;
  }
  const innerWorker = new Worker('tree-worker-wasm.js');
  window.supersize.worker = new TreeWorker(innerWorker, onProgressHandler);
  return window.supersize.worker;
}
