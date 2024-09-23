// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/ozone/public/ozone_platform.h"
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
  if (image.isNull()) {
    return false;
  }

  // Because we need a GL context per window, we do a quick check so that we
  // don't make another context if the window would just be displaying a mostly
  // transparent image.
  const SkBitmap* in_bitmap = image.bitmap();
  for (int y = 0; y < in_bitmap->height(); ++y) {
    for (int x = 0; x < in_bitmap->width(); ++x) {
      if (SkColorGetA(in_bitmap->getColor(x, y)) > kMinAlpha) {
        return true;
      }
    }
  }

  return false;
}

std::unique_ptr<Widget> CreateDragWidget(
    const gfx::Point& root_location,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& drag_widget_offset) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_DRAG);
  params.accept_events = false;
  params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;

  gfx::Point location = root_location - drag_widget_offset;
  params.bounds = gfx::Rect(location, image.size());
  widget->set_focus_on_creation(false);
  widget->set_frame_type(Widget::FrameType::kForceNative);
  widget->Init(std::move(params));
  widget->GetNativeWindow()->SetName("DragWindow");

  auto image_view = std::make_unique<ImageView>();
  image_view->SetImage(ui::ImageModel::FromImageSkia(image));
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
        (drag_data->GetSource() ? std::make_optional<ui::DataTransferEndpoint>(
                                      *drag_data->GetSource())
                                : std::nullopt),
        {drag_info.data_endpoint}, drag_data->GetFilenames(),
        std::move(drop_cb));
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
    auto output_drag_op = DragOperation::kNone;
    std::move(drop_cb).Run(std::move(data_to_drop), output_drag_op,
                           /*drag_image_layer_owner=*/nullptr);
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
  ResetDragDropTarget();
  observers_.Notify(
      &aura::client::DragDropClientObserver::OnDragDropClientDestroying);
}

DragOperation DesktopDragDropClientOzone::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  if (!drag_handler_) {
    return DragOperation::kNone;
  }

  DCHECK(!drag_context_);
  drag_context_ = std::make_unique<DragContext>();

  if (drag_handler_->ShouldReleaseCaptureForDrag(data.get())) {
    aura::Window* capture_window =
        aura::client::GetCaptureClient(root_window)->GetGlobalCaptureWindow();
    if (capture_window) {
      capture_window->ReleaseCapture();
    }
  }

  auto* cursor_client = aura::client::GetCursorClient(root_window);
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
      base::BindOnce(&DesktopDragDropClientOzone::OnDragStarted,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&DesktopDragDropClientOzone::OnDragFinished,
                     weak_factory_.GetWeakPtr()),
      GetLocationDelegate());

  if (!alive) {
    return DragOperation::kNone;
  }

  if (!drag_succeeded) {
    selected_operation_ = DragOperation::kNone;
    observers_.Notify(&aura::client::DragDropClientObserver::OnDragCancelled);
  }

  if (cursor_client) {
    cursor_client->SetCursor(initial_cursor);
  }
  drag_context_.reset();

  return selected_operation_;
}

#if BUILDFLAG(IS_LINUX)
void DesktopDragDropClientOzone::UpdateDragImage(const gfx::ImageSkia& image,
                                                 const gfx::Vector2d& offset) {
  DCHECK(drag_handler_);
  drag_handler_->UpdateDragImage(image, offset);
}
#endif  // BUILDFLAG(LINUX)

void DesktopDragDropClientOzone::DragCancel() {
  ResetDragDropTarget();
  selected_operation_ = DragOperation::kNone;

  if (!drag_handler_) {
    return;
  }
  drag_handler_->CancelDrag();
}

bool DesktopDragDropClientOzone::IsDragDropInProgress() {
  return drag_context_.get();
}

void DesktopDragDropClientOzone::AddObserver(
    aura::client::DragDropClientObserver* observer) {
  observers_.AddObserver(observer);
}

void DesktopDragDropClientOzone::RemoveObserver(
    aura::client::DragDropClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DesktopDragDropClientOzone::OnDragEnter(const gfx::PointF& location,
                                             int operations,
                                             int modifiers) {
  // Cache received values and wait for |data_to_drop_| to be delivered through
  // OnDragDataAvailable and then propagate drag events to `drag_drop_delegate_`
  // TODO(nickdiego): Check if delegates does/should really require drag data.
  drag_location_ = location;
  available_operations_ = operations;
  modifiers_ = modifiers;
}

void DesktopDragDropClientOzone::OnDragDataAvailable(
    std::unique_ptr<ui::OSExchangeData> data) {
  DCHECK(data);
  data_to_drop_ = std::move(data);
  std::unique_ptr<ui::DropTargetEvent> event = UpdateTargetAndCreateDropEvent();
  if (event) {
    observers_.Notify(&aura::client::DragDropClientObserver::OnDragUpdated,
                      *event);
  }
}

int DesktopDragDropClientOzone::OnDragMotion(const gfx::PointF& location,
                                             int operations,
                                             int modifiers) {
  drag_location_ = location;
  available_operations_ = operations;
  modifiers_ = modifiers;

  // If |data_to_drop_| doesn't have data, return that we accept everything.
  if (!data_to_drop_) {
    return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
  }

  // Ask the delegate what operation it would accept for the current data.
  int client_operation = ui::DragDropTypes::DRAG_NONE;
  auto event = UpdateTargetAndCreateDropEvent();
  if (event) {
    observers_.Notify(&aura::client::DragDropClientObserver::OnDragUpdated,
                      *event);
    if (delegate_) {
      current_drag_update_info_ = delegate_->OnDragUpdated(*event);
      client_operation = current_drag_update_info_.drag_operation;
    }
  }
  return client_operation;
}

void DesktopDragDropClientOzone::OnDragDrop(int modifiers) {
  modifiers_ = modifiers;
  // Ensure |data_to_drop_| is set, so crashes, such as
  // https://crbug.com/1151836, are avoided.
  if (data_to_drop_) {
    auto event = UpdateTargetAndCreateDropEvent();
    if (delegate_ && event) {
      if (auto drop_cb = delegate_->GetDropCallback(*event)) {
        base::ScopedClosureRunner drag_cancel(
            base::BindOnce(&DesktopDragDropClientOzone::DragCancel,
                           weak_factory_.GetWeakPtr()));

        auto* data_to_drop_raw = data_to_drop_.get();
        DropIfAllowed(
            data_to_drop_raw, current_drag_update_info_,
            base::BindOnce(&PerformDrop, std::move(drop_cb),
                           std::move(data_to_drop_), std::move(drag_cancel)));

        observers_.Notify(
            &aura::client::DragDropClientObserver::OnDragCompleted, *event);
      }
    }
  }
  ResetDragDropTarget(/*send_exit=*/false);
}

void DesktopDragDropClientOzone::OnDragLeave() {
  data_to_drop_.reset();
  drag_location_ = {};
  available_operations_ = 0;
  modifiers_ = 0;
  ResetDragDropTarget();
}

void DesktopDragDropClientOzone::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, entered_window_);

  entered_window_->RemoveObserver(this);
  entered_window_ = nullptr;
  delegate_ = nullptr;
  current_drag_update_info_ = aura::client::DragUpdateInfo();
}

ui::WmDragHandler::LocationDelegate*
DesktopDragDropClientOzone::GetLocationDelegate() {
  return nullptr;
}

void DesktopDragDropClientOzone::OnDragStarted() {
  observers_.Notify(&aura::client::DragDropClientObserver::OnDragStarted);
}

void DesktopDragDropClientOzone::OnDragFinished(DragOperation operation) {
  selected_operation_ = operation;
  observers_.Notify(&aura::client::DragDropClientObserver::OnDropCompleted,
                    operation);
}

std::unique_ptr<ui::DropTargetEvent>
DesktopDragDropClientOzone::UpdateTargetAndCreateDropEvent() {
  DCHECK(data_to_drop_);

  aura::Window* window = root_window_->GetEventHandlerForPoint(
      gfx::ToFlooredPoint(drag_location_));

  if (!window) {
    ResetDragDropTarget();
    return nullptr;
  }

  auto* new_delegate = aura::client::GetDragDropDelegate(window);
  const bool delegate_has_changed = (new_delegate != delegate_);
  if (delegate_has_changed) {
    ResetDragDropTarget();
    delegate_ = new_delegate;
    entered_window_ = window;
    entered_window_->AddObserver(this);
  }

  if (!delegate_) {
    return nullptr;
  }

  gfx::PointF target_location(drag_location_);
  aura::Window::ConvertPointToTarget(root_window_, window, &target_location);

  auto event = std::make_unique<ui::DropTargetEvent>(
      *data_to_drop_, target_location, gfx::PointF(drag_location_),
      available_operations_);
  event->SetFlags(modifiers_);
  if (delegate_has_changed) {
    delegate_->OnDragEntered(*event);
  }
  return event;
}

void DesktopDragDropClientOzone::ResetDragDropTarget(bool send_exit) {
  if (delegate_) {
    if (send_exit) {
      delegate_->OnDragExited();
    }
    delegate_ = nullptr;
  }
  if (entered_window_) {
    entered_window_->RemoveObserver(this);
    entered_window_ = nullptr;
  }
}

}  // namespace views
