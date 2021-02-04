// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

aura::Window* GetTargetWindow(aura::Window* root_window,
                              const gfx::Point& point) {
  gfx::Point root_location(point);
  root_window->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  return root_window->GetEventHandlerForPoint(root_location);
}

// The minimum alpha required so we would treat the pixel as visible.
constexpr uint32_t kMinAlpha = 32;

bool DragImageIsNeeded() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    return !ui::OzonePlatform::GetInstance()
                ->GetPlatformProperties()
                .platform_shows_drag_image;
  }
#endif
  return true;
}

// Returns true if |image| has any visible regions (defined as having a pixel
// with alpha > |kMinAlpha|).
bool IsValidDragImage(const gfx::ImageSkia& image) {
  if (image.isNull())
    return false;

  // Because we need a GL context per window, we do a quick check so that we
  // don't make another context if the window would just be displaying a mostly
  // transparent image.
  const SkBitmap* in_bitmap = image.bitmap();
  for (int y = 0; y < in_bitmap->height(); ++y) {
    uint32_t* in_row = in_bitmap->getAddr32(0, y);

    for (int x = 0; x < in_bitmap->width(); ++x) {
      if (SkColorGetA(in_row[x]) > kMinAlpha)
        return true;
    }
  }

  return false;
}

std::unique_ptr<views::Widget> CreateDragWidget(
    const gfx::Point& root_location,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& drag_widget_offset) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_DRAG);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.accept_events = false;

  gfx::Point location = root_location - drag_widget_offset;
  params.bounds = gfx::Rect(location, image.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(views::Widget::FrameType::kForceNative);
  widget->Init(std::move(params));
  widget->GetNativeWindow()->SetName("DragWindow");

  std::unique_ptr<views::ImageView> image_view =
      std::make_unique<views::ImageView>();
  image_view->SetImage(image);
  widget->SetContentsView(std::move(image_view));
  widget->Show();
  widget->GetNativeWindow()->layer()->SetFillsBoundsOpaquely(false);
  widget->StackAtTop();

  return widget;
}

}  // namespace

DesktopDragDropClientOzone::DragContext::DragContext() = default;

DesktopDragDropClientOzone::DragContext::~DragContext() = default;

DesktopDragDropClientOzone::DesktopDragDropClientOzone(
    aura::Window* root_window,
    ui::WmDragHandler* drag_handler)
    : root_window_(root_window),
      drag_handler_(drag_handler) {}

DesktopDragDropClientOzone::~DesktopDragDropClientOzone() {
  ResetDragDropTarget(true);
}

int DesktopDragDropClientOzone::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::mojom::DragEventSource source) {
  if (!drag_handler_)
    return ui::DragDropTypes::DragOperation::DRAG_NONE;

  DCHECK(!drag_context_);
  drag_context_ = std::make_unique<DragContext>();

  // Chrome expects starting drag and drop to release capture.
  aura::Window* capture_window =
      aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
  if (capture_window)
    capture_window->ReleaseCapture();

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  auto initial_cursor = source_window->GetHost()->last_cursor();
  drag_operation_ = operation;
  if (cursor_client) {
    cursor_client->SetCursor(ui::mojom::CursorType::kGrabbing);
  }

  if (DragImageIsNeeded()) {
    const auto& provider = data->provider();
    gfx::ImageSkia drag_image = provider.GetDragImage();
    if (IsValidDragImage(drag_image)) {
      drag_context_->size = drag_image.size();
      drag_context_->offset = provider.GetDragImageOffset();
      drag_context_->widget =
          CreateDragWidget(root_location, drag_image, drag_context_->offset);
    }
  }

  // This object is owned by a DesktopNativeWidgetAura that can be destroyed
  // during the drag loop, which will also destroy this object.  So keep track
  // of whether we are still alive after the drag ends.
  auto alive = weak_factory_.GetWeakPtr();

  const bool drag_succeeded = drag_handler_->StartDrag(
      *data.get(), operation, cursor_client->GetCursor(),
      !source_window->HasCapture(), this);

  if (!alive)
    return ui::DragDropTypes::DRAG_NONE;

  if (!drag_succeeded)
    drag_operation_ = ui::DragDropTypes::DRAG_NONE;

  if (cursor_client)
    cursor_client->SetCursor(initial_cursor);
  drag_context_.reset();

  return drag_operation_;
}

void DesktopDragDropClientOzone::DragCancel() {
  if (!drag_handler_)
    return;

  drag_handler_->CancelDrag();
  drag_operation_ = ui::DragDropTypes::DRAG_NONE;
}

bool DesktopDragDropClientOzone::IsDragDropInProgress() {
  return drag_context_.get();
}

void DesktopDragDropClientOzone::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DesktopDragDropClientOzone::OnDragEnter(
    const gfx::PointF& point,
    std::unique_ptr<ui::OSExchangeData> data,
    int operation,
    int modifiers) {
  last_drag_point_ = point;
  last_drop_operation_ = operation;

  // If |data| is empty, we defer sending any events to the
  // |drag_drop_delegate_|.  All necessary events will be sent on dropping.
  if (!data)
    return;

  data_to_drop_ = std::move(data);
  UpdateTargetAndCreateDropEvent(point, modifiers);
}

int DesktopDragDropClientOzone::OnDragMotion(const gfx::PointF& point,
                                             int operation,
                                             int modifiers) {
  last_drag_point_ = point;
  last_drop_operation_ = operation;

  // If |data_to_drop_| doesn't have data, return that we accept everything.
  if (!data_to_drop_)
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;

  // Ask the delegate what operation it would accept for the current data.
  int client_operation = ui::DragDropTypes::DRAG_NONE;
  std::unique_ptr<ui::DropTargetEvent> event =
      UpdateTargetAndCreateDropEvent(point, modifiers);
  if (drag_drop_delegate_ && event) {
    client_operation =
        drag_drop_delegate_->OnDragUpdated(*event).drag_operation;
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    int modifiers) {
  // If we didn't have |data_to_drop_|, then |drag_drop_delegate_| had never
  // been updated, and now it needs to receive deferred enter and update events
  // before handling the actual drop.
  const bool posponed_enter_and_update = !data_to_drop_;

  // If we didn't have |data_to_drop_| already since the drag had entered the
  // window, take the new data that comes now.
  if (!data_to_drop_)
    data_to_drop_ = std::move(data);

  // crbug.com/1151836: check that we have data.
  if (data_to_drop_) {
    // This will call the delegate's OnDragEntered if needed.
    auto event = UpdateTargetAndCreateDropEvent(last_drag_point_, modifiers);
    if (drag_drop_delegate_ && event) {
      if (posponed_enter_and_update) {
        // TODO(https://crbug.com/1014860): deal with drop refusals.
        // The delegate's OnDragUpdated returns an operation that the delegate
        // would accept.  Normally the accepted operation would be propagated
        // properly, and if the delegate didn't accept it, the drop would never
        // be called, but in this scenario of postponed updates we send all
        // events at once.  Now we just drop, but perhaps we could call
        // OnDragLeave and quit?
        drag_drop_delegate_->OnDragUpdated(*event);
      }
      drag_drop_delegate_->OnPerformDrop(*event, std::move(data_to_drop_));
    }
  }
  ResetDragDropTarget(false);
}

void DesktopDragDropClientOzone::OnDragLeave() {
  data_to_drop_.reset();
  ResetDragDropTarget(true);
}

void DesktopDragDropClientOzone::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, current_window_);

  current_window_->RemoveObserver(this);
  current_window_ = nullptr;
  drag_drop_delegate_ = nullptr;
}

void DesktopDragDropClientOzone::OnDragLocationChanged(
    const gfx::Point& screen_point_px) {
  DCHECK(drag_context_);

  if (!drag_context_->widget)
    return;

  const bool dispatch_mouse_event = !drag_context_->last_screen_location_px;
  drag_context_->last_screen_location_px = screen_point_px;
  if (dispatch_mouse_event) {
    // Post a task to dispatch mouse movement event when control returns to the
    // message loop. This allows smoother dragging since the events are
    // dispatched without waiting for the drag widget updates.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DesktopDragDropClientOzone::UpdateDragWidgetLocation,
                       weak_factory_.GetWeakPtr()));
  }
}

void DesktopDragDropClientOzone::OnDragOperationChanged(
    ui::DragDropTypes::DragOperation operation) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (!cursor_client)
    return;

  ui::mojom::CursorType cursor_type = ui::mojom::CursorType::kNull;
  switch (operation) {
    case ui::DragDropTypes::DRAG_NONE:
      cursor_type = ui::mojom::CursorType::kDndNone;
      break;
    case ui::DragDropTypes::DRAG_MOVE:
      cursor_type = ui::mojom::CursorType::kDndMove;
      break;
    case ui::DragDropTypes::DRAG_COPY:
      cursor_type = ui::mojom::CursorType::kDndCopy;
      break;
    case ui::DragDropTypes::DRAG_LINK:
      cursor_type = ui::mojom::CursorType::kDndLink;
      break;
  }
  cursor_client->SetCursor(cursor_type);
}

void DesktopDragDropClientOzone::OnDragFinished(int dnd_action) {
  drag_operation_ = dnd_action;
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::UpdateTargetAndCreateDropEvent(
    const gfx::PointF& location,
    int modifiers) {
  DCHECK(data_to_drop_);

  const gfx::Point point(location.x(), location.y());
  aura::Window* window = GetTargetWindow(root_window_, point);
  if (!window) {
    ResetDragDropTarget(true);
    return nullptr;
  }

  auto* new_delegate = aura::client::GetDragDropDelegate(window);
  const bool delegate_has_changed = (new_delegate != drag_drop_delegate_);
  if (delegate_has_changed) {
    ResetDragDropTarget(true);
    drag_drop_delegate_ = new_delegate;
    current_window_ = window;
    current_window_->AddObserver(this);
  }

  if (!drag_drop_delegate_)
    return nullptr;

  gfx::Point root_location(location.x(), location.y());
  root_window_->GetHost()->ConvertScreenInPixelsToDIP(&root_location);
  gfx::PointF target_location(root_location);
  aura::Window::ConvertPointToTarget(root_window_, window, &target_location);

  auto event = std::make_unique<ui::DropTargetEvent>(
      *data_to_drop_, target_location, gfx::PointF(root_location),
      last_drop_operation_);
  event->set_flags(modifiers);
  if (delegate_has_changed)
    drag_drop_delegate_->OnDragEntered(*event);
  return event;
}

void DesktopDragDropClientOzone::UpdateDragWidgetLocation() {
  if (!drag_context_)
    return;

  float scale_factor =
      ui::GetScaleFactorForNativeView(drag_context_->widget->GetNativeWindow());
  gfx::Point scaled_point = gfx::ScaleToRoundedPoint(
      *drag_context_->last_screen_location_px, 1.f / scale_factor);
  drag_context_->widget->SetBounds(
      gfx::Rect(scaled_point - drag_context_->offset, drag_context_->size));
  drag_context_->widget->StackAtTop();

  drag_context_->last_screen_location_px.reset();
}

void DesktopDragDropClientOzone::ResetDragDropTarget(bool send_exit) {
  if (drag_drop_delegate_) {
    if (send_exit)
      drag_drop_delegate_->OnDragExited();
    drag_drop_delegate_ = nullptr;
  }
  if (current_window_) {
    current_window_->RemoveObserver(this);
    current_window_ = nullptr;
  }
}

}  // namespace views
