// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/example_base.h"

#include "ui/views/view.h"

namespace views {
namespace examples {

ExampleBase::~ExampleBase() = default;

ExampleBase::ExampleBase(const char* title)
    : example_title_(title), container_(std::make_unique<View>()) {}

}  // namespace examples
}  // namespace views
