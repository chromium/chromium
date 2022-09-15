// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "heap/stubs.h"

namespace blink {

class WrongClass {};

class RightClass : GarbageCollected<WrongClass> {};

template <class T>
struct TemplatedClass : GarbageCollected<TemplatedClass<T>> {};

template struct TemplatedClass<int>;

template <class T1>
struct TemplatedClass2 : GarbageCollected<TemplatedClass2<T1>> {};

extern template struct TemplatedClass2<double>;
template struct TemplatedClass2<double>;

}  // namespace blink
