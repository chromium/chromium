// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test file for the v8_handle_migrate clang tool.

#include "base/handles.h"
#include "base/objects.h"

using v8::internal::DirectHandle;
using v8::internal::Handle;
using v8::internal::HeapObject;
using v8::internal::Map;
using v8::internal::Tagged;

template <typename T>
Tagged<Map> bar(Handle<T> h) {
  return **h == nullptr;
}

template <>
Tagged<Map> bar<HeapObject>(Handle<HeapObject> h) {
  return (*h)->map();
}
