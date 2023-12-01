// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export type ArrayDataModelSpliceEvent = CustomEvent<{
  removed: any[],
  added: any[],
  index?: number,
}>;

export type ArrayDataModelChangeEvent = CustomEvent<{
  index: number,
}>;
