// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAD_RAW_PTR_MAIN_H_
#define BAD_RAW_PTR_MAIN_H_

#include "base/memory/raw_ptr.h"

void BadCastInHeaderFile() {
  void* p = nullptr;

  // CK_BitCast via |static_cast| should emit an error.
  (void)static_cast<raw_ptr<int>*>(p);
}

#endif // BAD_RAW_PTR_MAIN_H_
