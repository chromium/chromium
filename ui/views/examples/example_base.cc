// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/example_base.h"

#include "ui/views/view.h"

namespace views::examples {

ExampleBase::~ExampleBase() = default;

ExampleBase::ExampleBase(const char* title) : example_title_(title) {}

}  // namespace views::examples
