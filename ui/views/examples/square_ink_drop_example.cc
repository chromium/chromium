// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/square_ink_drop_example.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/view.h"

namespace views::examples {

SquareInkDropExample::SquareInkDropExample()
    : InkDropExample("Square Ink Drop") {}

SquareInkDropExample::~SquareInkDropExample() = default;

void SquareInkDropExample::CreateInkDrop() {
  auto ink_drop_host = std::make_unique<InkDropHost>(ink_drop_view());
  ink_drop_host->SetMode(InkDropHost::InkDropMode::ON);
  InkDrop::UseInkDropForSquareRipple(ink_drop_host.get());
  ink_drop_host->SetCreateRippleCallback(base::BindRepeating(
      [](InkDropHost* ink_drop_host, SquareInkDropExample* example) {
        return ink_drop_host->CreateSquareRipple(
            example->ink_drop_view()->GetLocalBounds().CenterPoint(),
            example->ink_drop_view()->GetContentsBounds().size());
      },
      ink_drop_host.get(), base::Unretained(this)));
  InkDrop::Install(ink_drop_view(), std::move(ink_drop_host));
}

}  // namespace views::examples
