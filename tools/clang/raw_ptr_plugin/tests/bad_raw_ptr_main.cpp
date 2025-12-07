// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bad_raw_ptr_main.h"

#include "bad_raw_ptr_unrelated.h"
#include "base/memory/raw_ptr.h"

void BadCastInMainFile() {
  void* p = nullptr;

  // CK_BitCast via |static_cast| should emit an error.
  (void)static_cast<raw_ptr<int>*>(p);

  (void)[&p]() {
    (void)static_cast<raw_ptr<int>*>(p);
  };
}
