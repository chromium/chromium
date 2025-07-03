// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAD_RAW_PTR_UNRELATED_H_
#define BAD_RAW_PTR_UNRELATED_H_

#include "base/memory/raw_ptr.h"

void BadCastInUnrelatedHeaderFile() {
  void* p = nullptr;

  // TODO(crbug.com/425542181): currently, this emits an error. In the future,
  // it'd be nice if we could skip scanning this file entirely.
  (void)static_cast<raw_ptr<int>*>(p);
}

#endif // BAD_RAW_PTR_UNRELATED_H_
