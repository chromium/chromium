// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PromiseResolver is a helper class that allows creating a
 * Promise that will be fulfilled (resolved or rejected) some time later.
 *
 * Example:
 *  const resolver = new PromiseResolver();
 *  resolver.promise.then(function(result) {
 *    console.log('resolved with', result);
 *  });
 *  ...
 *  ...
 *  resolver.resolve({hello: 'world'});
 */

export class PromiseResolver<T> {
  private resolve_: (arg: T) => void = () => {};
  private reject_: (arg: any) => void = () => {};
  private isFulfilled_: boolean = false;
  private promise_: Promise<T>;

  constructor() {
    this.promise_ = new Promise((resolve, reject) => {
      this.resolve_ = (resolution: T) => {
        resolve(resolution);
        this.isFulfilled_ = true;
      };
      this.reject_ = (reason: any) => {
        reject(reason);
        this.isFulfilled_ = true;
      };
    });
  }

  /** Whether this resolver has been resolved or rejected. */
  get isFulfilled(): boolean {
    return this.isFulfilled_;
  }

  get promise(): Promise<T> {
    return this.promise_;
  }

  get resolve(): ((arg: T) => void) {
    return this.resolve_;
  }

  get reject(): ((arg?: any) => void) {
    return this.reject_;
  }
}
