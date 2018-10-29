// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

namespace {

class FakeWmDragHandler;

// A fake handler, which initiates dragging.
class FakeWmDragHandler : public ui::WmDragHandler {
 public:
  FakeWmDragHandler() : weak_ptr_factory_(this) {}
  ~FakeWmDragHandler() override = default;

  // ui::WmDragHandler
  void StartDrag(const OSExchangeData& data,
                 const int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override {
    callback_ = std::move(callback);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::OnceCallback<void(int)> callback) {
                         std::move(callback).Run(ui::DragDropTypes::DRAG_COPY);
                       },
                       std::move(callback_)));
  }

 private:
  base::OnceCallback<void(int)> callback_;
  base::WeakPtrFactory<FakeWmDragHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeWmDragHandler);
};

}  // namespace

class DesktopDragDropClientOzoneTest : public ViewsTestBase {
 public:
  DesktopDragDropClientOzoneTest() = default;
  ~DesktopDragDropClientOzoneTest() override = default;

  int StartDragAndDrop() {
    ui::OSExchangeData data;
    data.SetString(base::ASCIIToUTF16("Test"));
    SkBitmap drag_bitmap;
    drag_bitmap.allocN32Pixels(10, 10);
    drag_bitmap.eraseARGB(0xFF, 0, 0, 0);
    gfx::ImageSkia drag_image(gfx::ImageSkia::CreateFrom1xBitmap(drag_bitmap));
    data.provider().SetDragImage(drag_image, gfx::Vector2d());

    return client_->StartDragAndDrop(
        data, widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(),
        ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    ViewsTestBase::SetUp();
    test_views_delegate()->set_use_desktop_native_widgets(true);

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new DesktopNativeWidgetAura(widget_.get());
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(params);
    widget_->Show();

    aura::Window* window = widget_->GetNativeWindow();
    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();
    drag_handler_ = std::make_unique<FakeWmDragHandler>();
    client_ = std::make_unique<DesktopDragDropClientOzone>(
        window, cursor_manager_.get(), drag_handler_.get());
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    drag_handler_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 private:
  std::unique_ptr<DesktopDragDropClientOzone> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;
  std::unique_ptr<FakeWmDragHandler> drag_handler_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientOzoneTest);
};

TEST_F(DesktopDragDropClientOzoneTest, StartDrag) {
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

}  // namespace views
