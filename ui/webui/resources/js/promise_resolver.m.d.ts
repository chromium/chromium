// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dpapad): Auto-generate this file when PromiseResolver is declared as
// "class PromiseResolver {...}", instead of
// "var PromiseResolver = class {...}".

export class PromiseResolver<T> {
  private isFulfilled_;
  private promise_;
  resolve_: (resolution?: T|undefined) => void;
  reject_: (reason?: any|undefined) => void;
  set isFulfilled(arg: boolean);
  get isFulfilled(): boolean;
  set promise(arg: Promise<T>);
  get promise(): Promise<T>;
  set resolve(arg: (arg0?: T|undefined) => void);
  get resolve(): (arg0?: T|undefined) => void;
  set reject(arg: () => void);
  get reject(): () => void;
}
