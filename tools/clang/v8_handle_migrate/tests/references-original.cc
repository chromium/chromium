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

void consume_handle(const Handle<HeapObject>& o);
void consume_direct(const DirectHandle<HeapObject>& o);

void f1(const Handle<HeapObject>& a, Handle<HeapObject>& b) {
  auto t1 = *a;
  auto t2 = *b;
  auto m1 = a->map();
  auto m2 = b->map();
  consume_handle(a);
  consume_direct(b);
}

void f2(const Handle<HeapObject>& a, Handle<HeapObject>& b) {
  auto t1 = *a;
  auto t2 = *b;
  auto m1 = a->map();
  auto m2 = b->map();
  consume_direct(a);
  consume_handle(b);
}

void parameter() {
  Handle<HeapObject> h1;
  Handle<HeapObject> h2;
  f1(h1, h2);
  f2(h1, h2);
}
