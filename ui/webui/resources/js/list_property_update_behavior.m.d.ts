// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ListPropertyUpdateBehavior {
  updateList(
      propertyPath: string,
      identityGetter: ((arg0: any) => (any | string)),
      updatedList: object[], identityBasedUpdate?: boolean): boolean;
}

declare const ListPropertyUpdateBehavior: object;
