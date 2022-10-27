// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/layout.h"
#include "ui/compositor/layer.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using ::ui::mojom::DragOperation;

// The minimum alpha required so we would treat the pixel as visible.
constexpr uint32_t kMinAlpha = 32;

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

// Drops the dragged data if no policy restrictions exist (Data Leak Prevention
// stack uninitialised) or if there are rules allowing the data transfer.
// Otherwise the drag is cancelled.
void DropIfAllowed(const ui::OSExchangeData* drag_data,
                   aura::client::DragUpdateInfo& drag_info,
                   base::OnceClosure drop_cb) {
  if (ui::DataTransferPolicyController::HasInstance()) {
    ui::DataTransferPolicyController::Get()->DropIfAllowed(
        drag_data->GetSource(), &drag_info.data_endpoint, std::move(drop_cb));
  } else {
    std::move(drop_cb).Run();
  }
}

// A callback that runs the drop closure, if there is one, to perform the data
// drop. If this callback is destroyed without running, the |drag_cancel|
// closure will run.
void PerformDrop(aura::client::DragDropDelegate::DropCallback drop_cb,
                 std::unique_ptr<ui::OSExchangeData> data_to_drop,
                 base::ScopedClosureRunner drag_cancel) {
  if (drop_cb) {
    auto output_drag_op = ui::mojom::DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data_to_drop), output_drag_op);
  }

  base::IgnoreResult(drag_cancel.Release());
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

DragOperation DesktopDragDropClientOzone::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  if (!drag_handler_)
    return DragOperation::kNone;

  DCHECK(!drag_context_);
  drag_context_ = std::make_unique<DragContext>();

  if (drag_handler_->ShouldReleaseCaptureForDrag(data.get())) {
    aura::Window* capture_window =
        aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
    if (capture_window) {
      capture_window->ReleaseCapture();
    }
  }

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  auto initial_cursor = source_window->GetHost()->last_cursor();
  if (cursor_client) {
    cursor_client->SetCursor(ui::mojom::CursorType::kGrabbing);
  }

  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .platform_shows_drag_image) {
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
      *data.get(), allowed_operations, source, cursor_client->GetCursor(),
      !source_window->HasCapture(),
      base::BindOnce(&DesktopDragDropClientOzone::OnDragFinished,
                     weak_factory_.GetWeakPtr()),
      GetLocationDelegate());

  if (!alive)
    return DragOperation::kNone;

  if (!drag_succeeded)
    drag_operation_ = DragOperation::kNone;

  if (cursor_client)
    cursor_client->SetCursor(initial_cursor);
  drag_context_.reset();

  return drag_operation_;
}

#if BUILDFLAG(IS_LINUX)
void DesktopDragDropClientOzone::UpdateDragImage(const gfx::ImageSkia& image,
                                                 const gfx::Vector2d& offset) {
  DCHECK(drag_handler_);
  drag_handler_->UpdateDragImage(image, offset);
}
#endif  // BUILDFLAG(LINUX)

void DesktopDragDropClientOzone::DragCancel() {
  ResetDragDropTarget(true);
  drag_operation_ = DragOperation::kNone;

  if (!drag_handler_)
    return;

  drag_handler_->CancelDrag();
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
    current_drag_info_ = drag_drop_delegate_->OnDragUpdated(*event);
    client_operation = current_drag_info_.drag_operation;
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    int modifiers) {
  // If we didn't have |data_to_drop_|, then |drag_drop_delegate_| had never
  // been updated, and now it needs to receive deferred enter and update events
  // before handling the actual drop.
  const bool postponed_enter_and_update = !data_to_drop_;

  // If we didn't have |data_to_drop_| already since the drag had entered the
  // window, take the new data that comes now.
  if (!data_to_drop_)
    data_to_drop_ = std::move(data);

  // crbug.com/1151836: check that we have data.
  if (data_to_drop_) {
    // This will call the delegate's OnDragEntered if needed.
    auto event = UpdateTargetAndCreateDropEvent(last_drag_point_, modifiers);
    if (drag_drop_delegate_ && event) {
      if (postponed_enter_and_update) {
        // TODO(https://crbug.com/1014860): deal with drop refusals.
        // The delegate's OnDragUpdated returns an operation that the delegate
        // would accept.  Normally the accepted operation would be propagated
        // properly, and if the delegate didn't accept it, the drop would never
        // be called, but in this scenario of postponed updates we send all
        // events at once.  Now we just drop, but perhaps we could call
        // OnDragLeave and quit?
        current_drag_info_ = drag_drop_delegate_->OnDragUpdated(*event);
      }
      auto drop_cb = drag_drop_delegate_->GetDropCallback(*event);
      if (drop_cb) {
        base::ScopedClosureRunner drag_cancel(
            base::BindOnce(&DesktopDragDropClientOzone::DragCancel,
                           weak_factory_.GetWeakPtr()));

        DropIfAllowed(
            data_to_drop_.get(), current_drag_info_,
            base::BindOnce(&PerformDrop, std::move(drop_cb),
                           std::move(data_to_drop_), std::move(drag_cancel)));
      }
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
  current_drag_info_ = aura::client::DragUpdateInfo();
}

ui::WmDragHandler::LocationDelegate*
DesktopDragDropClientOzone::GetLocationDelegate() {
  return nullptr;
}

void DesktopDragDropClientOzone::OnDragFinished(DragOperation operation) {
  drag_operation_ = operation;
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::UpdateTargetAndCreateDropEvent(
    const gfx::PointF& root_location,
    int modifiers) {
  DCHECK(data_to_drop_);

  aura::Window* window =
      root_window_->GetEventHandlerForPoint(gfx::ToFlooredPoint(root_location));

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
