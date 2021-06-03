// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ListPropertyUpdateBehaviorInterface {
  updateList(
      propertyPath: string,
      identityGetter: ((arg0: object) => (object | string)),
      updatedList: object[], identityBasedUpdate?: boolean): boolean;
}

export {ListPropertyUpdateBehavior};

interface ListPropertyUpdateBehavior extends
    ListPropertyUpdateBehaviorInterface {}

declare const ListPropertyUpdateBehavior: object;
