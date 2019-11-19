// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_test_base.h"

#include "base/macros.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

// Testing wrapper of the NativeViewHost.
class NativeViewHostTestBase::NativeViewHostTesting : public NativeViewHost {
 public:
  explicit NativeViewHostTesting(NativeViewHostTestBase* owner)
      : owner_(owner) {}
  ~NativeViewHostTesting() override { owner_->host_destroyed_count_++; }

 private:
  NativeViewHostTestBase* owner_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostTesting);
};

NativeViewHostTestBase::NativeViewHostTestBase() = default;

NativeViewHostTestBase::~NativeViewHostTestBase() = default;

void NativeViewHostTestBase::TearDown() {
  DestroyTopLevel();
  ViewsTestBase::TearDown();
}

void NativeViewHostTestBase::CreateTopLevel() {
  toplevel_ = std::make_unique<Widget>();
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel_->Init(std::move(toplevel_params));
}

void NativeViewHostTestBase::CreateTestingHost() {
  host_ = std::make_unique<NativeViewHostTesting>(this);
}

Widget* NativeViewHostTestBase::CreateChildForHost(
    gfx::NativeView native_parent_view,
    View* parent_view,
    View* contents_view,
    NativeViewHost* host) {
  Widget* child = new Widget;
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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

}  // namespace test
}  // namespace views
