// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assertNotReached} from './assert.m.js';

/**
 * @fileoverview PromiseResolver is a helper class that allows creating a
 * Promise that will be fulfilled (resolved or rejected) some time later.
 *
 * Example:
 *  var resolver = new PromiseResolver();
 *  resolver.promise.then(function(result) {
 *    console.log('resolved with', result);
 *  });
 *  ...
 *  ...
 *  resolver.resolve({hello: 'world'});
 */

/** @template T */
// eslint-disable-next-line no-var
/* #export */ var PromiseResolver = class {
  constructor() {
    /** @private {function(T=): void} */
    this.resolve_;

    /** @private {function(*=): void} */
    this.reject_;

    /** @private {boolean} */
    this.isFulfilled_ = false;

    /** @private {!Promise<T>} */
    this.promise_ = new Promise((resolve, reject) => {
      this.resolve_ = /** @param {T=} resolution */ (resolution) => {
        resolve(resolution);
        this.isFulfilled_ = true;
      };
      this.reject_ = /** @param {*=} reason */ (reason) => {
        reject(reason);
        this.isFulfilled_ = true;
      };
    });
  }

  /** @return {boolean} Whether this resolver has been resolved or rejected. */
  get isFulfilled() {
    return this.isFulfilled_;
  }

  set isFulfilled(i) {
    assertNotReached();
  }

  /** @return {!Promise<T>} */
  get promise() {
    return this.promise_;
  }

  set promise(p) {
    assertNotReached();
  }

  /** @return {function(T=): void} */
  get resolve() {
    return this.resolve_;
  }

  set resolve(r) {
    assertNotReached();
  }

  /** @return {function(*=): void} */
  get reject() {
    return this.reject_;
  }

  set reject(s) {
    assertNotReached();
  }
};
