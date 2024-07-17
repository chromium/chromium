// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for the v8_handle_migrate clang tool.

#include <utility>

#include "base/handles.h"
#include "base/objects.h"

using v8::internal::DirectHandle;
using v8::internal::Handle;
using v8::internal::HeapObject;

void consume_handle(Handle<HeapObject> o);
void consume_direct(DirectHandle<HeapObject> o);

void auto_destructuring() {
  std::pair<Handle<HeapObject>, Handle<HeapObject>> p;
  auto [h1, h2] = p;
  consume_direct(h1);
  consume_direct(h2);
  consume_handle(h2);
}
