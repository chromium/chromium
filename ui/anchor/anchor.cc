// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/anchor/anchor.h"

#include <memory>
#include <utility>

#include "base/check.h"

namespace {

class EmptyAnchor : public ui::AnchorImpl {
 public:
  ~EmptyAnchor() override = default;

  // ui::AnchorImpl:
  std::unique_ptr<AnchorImpl> Clone() const override {
    return std::make_unique<EmptyAnchor>();
  }
  bool IsEmpty() const override { return true; }
  gfx::Rect GetScreenBounds() const override { return gfx::Rect(); }
  views::Widget* GetWidget() override { return nullptr; }
  bool IsView() const override { return false; }
  views::View* GetView() override { return nullptr; }
};

}  // namespace

namespace ui {

Anchor::Anchor() : impl_(std::make_unique<EmptyAnchor>()) {}

Anchor::Anchor(std::unique_ptr<AnchorImpl> impl) : impl_(std::move(impl)) {
  CHECK(impl_);
}

Anchor::Anchor(const Anchor& other) : impl_(other.impl_->Clone()) {}

Anchor::Anchor(Anchor&& other) noexcept
    : impl_(std::exchange(other.impl_, std::make_unique<EmptyAnchor>())) {}

Anchor& Anchor::operator=(const Anchor& other) {
  if (this != &other) {
    impl_ = other.impl_->Clone();
  }
  return *this;
}

Anchor& Anchor::operator=(Anchor&& other) noexcept {
  if (this != &other) {
    impl_ = std::exchange(other.impl_, std::make_unique<EmptyAnchor>());
  }
  return *this;
}

Anchor::~Anchor() = default;

bool Anchor::IsView() const {
  return impl_->IsView();
}

views::View* Anchor::GetView() {
  return impl_->GetView();
}

bool Anchor::IsEmpty() const {
  return impl_->IsEmpty();
}

gfx::Rect Anchor::GetScreenBounds() const {
  return impl_->GetScreenBounds();
}

views::Widget* Anchor::GetWidget() {
  return impl_->GetWidget();
}

}  // namespace ui
