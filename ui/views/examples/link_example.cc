// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/link_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

LinkExample::LinkExample() : ExampleBase("Link") {
}

LinkExample::~LinkExample() = default;

void LinkExample::CreateExampleView(View* container) {
  link_ = new Link(base::ASCIIToUTF16("Click me!"));
  link_->set_listener(this);

  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(link_);
}

void LinkExample::LinkClicked(Link* source, int event_flags) {
  PrintStatus("Link clicked");
}

}  // namespace examples
}  // namespace views
