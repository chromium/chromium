// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_test_base.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace views::test {

// Testing wrapper of the NativeViewHost.
class NativeViewHostTestBase::NativeViewHostTesting : public NativeViewHost {
 public:
  explicit NativeViewHostTesting(NativeViewHostTestBase* owner)
      : owner_(owner) {}

  NativeViewHostTesting(const NativeViewHostTesting&) = delete;
  NativeViewHostTesting& operator=(const NativeViewHostTesting&) = delete;

  ~NativeViewHostTesting() override { owner_->host_destroyed_count_++; }

  // NativeViewHost:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    ++owner_->on_mouse_pressed_called_count_;
    return NativeViewHost::OnMousePressed(event);
  }

 private:
  raw_ptr<NativeViewHostTestBase> owner_;
};

NativeViewHostTestBase::NativeViewHostTestBase() = default;

NativeViewHostTestBase::~NativeViewHostTestBase() = default;

void NativeViewHostTestBase::TearDown() {
  DestroyTopLevel();
  ViewsTestBase::TearDown();
}

void NativeViewHostTestBase::CreateTopLevel(WidgetDelegate* widget_delegate) {
  toplevel_ = std::make_unique<Widget>();
  Widget::InitParams toplevel_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  toplevel_params.delegate = widget_delegate;
  toplevel_->Init(std::move(toplevel_params));
}

void NativeViewHostTestBase::CreateTestingHost() {
  host_ = std::make_unique<NativeViewHostTesting>(this);
}

std::unique_ptr<Widget> NativeViewHostTestBase::CreateChildForHost(
    gfx::NativeView native_parent_view,
    View* parent_view,
    View* contents_view,
    NativeViewHost* host) {
  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
  child_params.parent = native_parent_view;
  child->Init(std::move(child_params));
  child->SetContentsView(contents_view);

  // Owned by |parent_view|.
  parent_view->AddChildView(host);
  host->Attach(child->GetNativeView());

  return child;
}

void NativeViewHostTestBase::DestroyTopLevel() {
  toplevel_.reset();
}

void NativeViewHostTestBase::DestroyHost() {
  host_.reset();
}

NativeViewHost* NativeViewHostTestBase::ReleaseHost() {
  return host_.release();
}

NativeViewHostWrapper* NativeViewHostTestBase::GetNativeWrapper() {
  return host_->native_wrapper_.get();
}

}  // namespace views::test
