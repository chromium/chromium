// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_handler/wm_drag_handler.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

namespace {
class FakePlatformWindow : public ui::PlatformWindowBase,
                           public ui::WmDragHandler {
 public:
  FakePlatformWindow() { SetWmDragHandler(this, this); }
  ~FakePlatformWindow() override = default;

  // ui::PlatformWindow
  void Show(bool inactive) override {}
  void Hide() override {}
  void Close() override {}
  bool IsVisible() const override { return true; }
  void PrepareForShutdown() override {}
  void SetBounds(const gfx::Rect& bounds) override {}
  gfx::Rect GetBounds() override { return gfx::Rect(); }
  void SetTitle(const base::string16& title) override {}
  void SetCapture() override {}
  void ReleaseCapture() override {}
  bool HasCapture() const override { return false; }
  void ToggleFullscreen() override {}
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  ui::PlatformWindowState GetPlatformWindowState() const override {
    return ui::PlatformWindowState::kNormal;
  }
  void Activate() override {}
  void Deactivate() override {}
  void SetCursor(ui::PlatformCursor cursor) override {}
  void MoveCursorTo(const gfx::Point& location) override {}
  void ConfineCursorToBounds(const gfx::Rect& bounds) override {}
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override {}
  gfx::Rect GetRestoredBoundsInPixels() const override { return gfx::Rect(); }
  void SetUseNativeFrame(bool use_native_frame) override {}
  bool ShouldUseNativeFrame() const override { return false; }
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override {}
  void SizeConstraintsChanged() override {}

  // ui::WmDragHandler
  void StartDrag(const OSExchangeData& data,
                 int operation,
                 gfx::NativeCursor cursor,
                 base::OnceCallback<void(int)> callback) override {
    drag_closed_callback_ = std::move(callback);
    source_data_ = std::make_unique<OSExchangeData>(data.provider().Clone());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakePlatformWindow::ProcessDrag, base::Unretained(this),
                       std::move(source_data_), operation));
  }

  void OnDragEnter(const gfx::PointF& point,
                   std::unique_ptr<OSExchangeData> data,
                   int operation) {
    ui::WmDropHandler* drop_handler = ui::GetWmDropHandler(*this);
    if (!drop_handler)
      return;
    drop_handler->OnDragEnter(point, std::move(data), operation);
  }

  int OnDragMotion(const gfx::PointF& point, int operation) {
    ui::WmDropHandler* drop_handler = ui::GetWmDropHandler(*this);
    if (!drop_handler)
      return 0;

    return drop_handler->OnDragMotion(point, operation);
  }

  void OnDragDrop(std::unique_ptr<OSExchangeData> data) {
    ui::WmDropHandler* drop_handler = ui::GetWmDropHandler(*this);
    if (!drop_handler)
      return;
    drop_handler->OnDragDrop(std::move(data));
  }

  void OnDragLeave() {
    ui::WmDropHandler* drop_handler = ui::GetWmDropHandler(*this);
    if (!drop_handler)
      return;
    drop_handler->OnDragLeave();
  }

  void CloseDrag(uint32_t dnd_action) {
    std::move(drag_closed_callback_).Run(dnd_action);
  }

  void ProcessDrag(std::unique_ptr<OSExchangeData> data, int operation) {
    OnDragEnter(gfx::PointF(), std::move(data), operation);
    int updated_operation = OnDragMotion(gfx::PointF(), operation);
    OnDragDrop(nullptr);
    OnDragLeave();
    CloseDrag(updated_operation);
  }

 private:
  base::OnceCallback<void(int)> drag_closed_callback_;
  std::unique_ptr<ui::OSExchangeData> source_data_;

  DISALLOW_COPY_AND_ASSIGN(FakePlatformWindow);
};

// DragDropDelegate which counts the number of each type of drag-drop event.
class FakeDragDropDelegate : public aura::client::DragDropDelegate {
 public:
  FakeDragDropDelegate()
      : num_enters_(0), num_updates_(0), num_exits_(0), num_drops_(0) {}
  ~FakeDragDropDelegate() override = default;

  int num_enters() const { return num_enters_; }
  int num_updates() const { return num_updates_; }
  int num_exits() const { return num_exits_; }
  int num_drops() const { return num_drops_; }
  ui::OSExchangeData* received_data() const { return received_data_.get(); }

  void SetOperation(int operation) { destination_operation_ = operation; }

 private:
  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {
    ++num_enters_;
  }

  int OnDragUpdated(const ui::DropTargetEvent& event) override {
    ++num_updates_;
    return destination_operation_;
  }

  void OnDragExited() override { ++num_exits_; }

  int OnPerformDrop(const ui::DropTargetEvent& event,
                    std::unique_ptr<ui::OSExchangeData> data) override {
    ++num_drops_;
    received_data_ = std::move(data);
    return destination_operation_;
  }

  int num_enters_;
  int num_updates_;
  int num_exits_;
  int num_drops_;
  std::unique_ptr<ui::OSExchangeData> received_data_;
  int destination_operation_;

  DISALLOW_COPY_AND_ASSIGN(FakeDragDropDelegate);
};

}  // namespace

class DesktopDragDropClientOzoneTest : public ViewsTestBase {
 public:
  DesktopDragDropClientOzoneTest() = default;
  ~DesktopDragDropClientOzoneTest() override = default;

  int StartDragAndDrop(int operation) {
    auto data = std::make_unique<ui::OSExchangeData>();
    data->SetString(base::ASCIIToUTF16("Test"));
    SkBitmap drag_bitmap;
    drag_bitmap.allocN32Pixels(10, 10);
    drag_bitmap.eraseARGB(0xFF, 0, 0, 0);
    gfx::ImageSkia drag_image(gfx::ImageSkia::CreateFrom1xBitmap(drag_bitmap));
    data->provider().SetDragImage(drag_image, gfx::Vector2d());

    return client_->StartDragAndDrop(
        std::move(data), widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(), gfx::Point(), operation,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(std::move(params));
    widget_->Show();

    // Creates FakeDragDropDelegate and set it for |window|.
    aura::Window* window = widget_->GetNativeWindow();
    dragdrop_delegate_ = std::make_unique<FakeDragDropDelegate>();
    aura::client::SetDragDropDelegate(window, dragdrop_delegate_.get());

    cursor_manager_ = std::make_unique<DesktopNativeCursorManager>();
    platform_window_ = std::make_unique<FakePlatformWindow>();
    ui::WmDragHandler* drag_handler = ui::GetWmDragHandler(*(platform_window_));
    // Creates DesktopDragDropClientOzone with |window| and |drag_handler|.
    client_ = std::make_unique<DesktopDragDropClientOzone>(
        window, cursor_manager_.get(), drag_handler);
    SetWmDropHandler(platform_window_.get(), client_.get());
  }

  void TearDown() override {
    client_.reset();
    cursor_manager_.reset();
    platform_window_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<FakeDragDropDelegate> dragdrop_delegate_;
  std::unique_ptr<FakePlatformWindow> platform_window_;

 private:
  std::unique_ptr<DesktopDragDropClientOzone> client_;
  std::unique_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientOzoneTest);
};

TEST_F(DesktopDragDropClientOzoneTest, StartDrag) {
  // Set the operation which the destination can accept.
  dragdrop_delegate_->SetOperation(ui::DragDropTypes::DRAG_COPY);
  // Start Drag and Drop with the operations suggested.
  int operation = StartDragAndDrop(ui::DragDropTypes::DRAG_COPY |
                                   ui::DragDropTypes::DRAG_MOVE);
  // The |operation| decided through negotiation should be 'DRAG_COPY'.
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, operation);

  EXPECT_EQ(1, dragdrop_delegate_->num_enters());
  EXPECT_EQ(1, dragdrop_delegate_->num_updates());
  EXPECT_EQ(1, dragdrop_delegate_->num_drops());
  EXPECT_EQ(1, dragdrop_delegate_->num_exits());
}

TEST_F(DesktopDragDropClientOzoneTest, ReceiveDrag) {
  // Set the operation which the destination can accept.
  int operation = ui::DragDropTypes::DRAG_MOVE;
  dragdrop_delegate_->SetOperation(operation);

  // Set the data which will be delivered.
  const base::string16 sample_data = base::ASCIIToUTF16("ReceiveDrag");
  std::unique_ptr<ui::OSExchangeData> data =
      std::make_unique<ui::OSExchangeData>();
  data->SetString(sample_data);

  // Simulate that the drag enter/motion/drop/leave events happen with the
  // |suggested_operation|.
  int suggested_operation =
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
  platform_window_->OnDragEnter(gfx::PointF(), std::move(data),
                                suggested_operation);
  int updated_operation =
      platform_window_->OnDragMotion(gfx::PointF(), suggested_operation);
  platform_window_->OnDragDrop(nullptr);
  platform_window_->OnDragLeave();

  // The |updated_operation| decided through negotiation should be
  // 'ui::DragDropTypes::DRAG_MOVE'.
  EXPECT_EQ(operation, updated_operation);

  base::string16 string_data;
  dragdrop_delegate_->received_data()->GetString(&string_data);
  EXPECT_EQ(sample_data, string_data);

  EXPECT_EQ(1, dragdrop_delegate_->num_enters());
  EXPECT_EQ(1, dragdrop_delegate_->num_enters());
  EXPECT_EQ(1, dragdrop_delegate_->num_updates());
  EXPECT_EQ(1, dragdrop_delegate_->num_drops());
  EXPECT_EQ(1, dragdrop_delegate_->num_exits());
}

}  // namespace views
