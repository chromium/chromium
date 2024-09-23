// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/drop_helper.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using ::ui::mojom::DragOperation;

const View* g_drag_entered_callback_view = nullptr;

base::RepeatingClosure* GetDragEnteredCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return callback.get();
}

}  // namespace

DropHelper::DropHelper(View* root_view) : root_view_(root_view) {}

DropHelper::~DropHelper() = default;

// static
void DropHelper::SetDragEnteredCallbackForTesting(
    const View* view,
    base::RepeatingClosure callback) {
  g_drag_entered_callback_view = view;
  *GetDragEnteredCallback() = std::move(callback);
}

void DropHelper::ResetTargetViewIfEquals(View* view) {
  if (target_view_ == view)
    target_view_ = nullptr;
  if (deepest_view_ == view)
    deepest_view_ = nullptr;
}

int DropHelper::OnDragOver(const OSExchangeData& data,
                           const gfx::Point& root_view_location,
                           int drag_operation) {
  const View* old_deepest_view = deepest_view_;
  View* view =
      CalculateTargetViewImpl(root_view_location, data, true, &deepest_view_);

  if (view != target_view_) {
    // Target changed. Notify old drag exited, then new drag entered.
    NotifyDragExit();
    target_view_ = view;
    NotifyDragEntered(data, root_view_location, drag_operation);
  }

  // Notify testing callback if the drag newly moved over the target view.
  if (g_drag_entered_callback_view &&
      g_drag_entered_callback_view->Contains(deepest_view_) &&
      !g_drag_entered_callback_view->Contains(old_deepest_view)) {
    auto* callback = GetDragEnteredCallback();
    if (!callback->is_null())
      callback->Run();
  }

  return NotifyDragOver(data, root_view_location, drag_operation);
}

void DropHelper::OnDragExit() {
  NotifyDragExit();
  deepest_view_ = target_view_ = nullptr;
}

DragOperation DropHelper::OnDrop(const OSExchangeData& data,
                                 const gfx::Point& root_view_location,
                                 int drag_operation) {
  View* drop_view = target_view_;
  deepest_view_ = target_view_ = nullptr;
  if (!drop_view)
    return DragOperation::kNone;

  if (drag_operation == ui::DragDropTypes::DRAG_NONE) {
    drop_view->OnDragExited();
    return DragOperation::kNone;
  }

  gfx::Point view_location(root_view_location);
  View* root_view = drop_view->GetWidget()->GetRootView();
  View::ConvertPointToTarget(root_view, drop_view, &view_location);
  ui::DropTargetEvent drop_event(data, gfx::PointF(view_location),
                                 gfx::PointF(root_view_location),
                                 drag_operation);
  auto output_drag_op = ui::mojom::DragOperation::kNone;
  auto drop_cb = drop_view->GetDropCallback(drop_event);
  std::move(drop_cb).Run(drop_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  return output_drag_op;
}

DropHelper::DropCallback DropHelper::GetDropCallback(
    const OSExchangeData& data,
    const gfx::Point& root_view_location,
    int drag_operation) {
  View* drop_view = target_view_;
  deepest_view_ = target_view_ = nullptr;
  if (!drop_view)
    return base::NullCallback();

  if (drag_operation == ui::DragDropTypes::DRAG_NONE) {
    drop_view->OnDragExited();
    return base::NullCallback();
  }

  gfx::Point view_location(root_view_location);
  View* root_view = drop_view->GetWidget()->GetRootView();
  View::ConvertPointToTarget(root_view, drop_view, &view_location);
  ui::DropTargetEvent drop_event(data, gfx::PointF(view_location),
                                 gfx::PointF(root_view_location),
                                 drag_operation);

  auto drop_view_cb = drop_view->GetDropCallback(drop_event);
  if (!drop_view_cb)
    return base::NullCallback();

  return base::BindOnce(
      [](const ui::DropTargetEvent& drop_event, View::DropCallback drop_cb,
         std::unique_ptr<ui::OSExchangeData> data,
         ui::mojom::DragOperation& output_drag_op,
         std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
        // Bind the drop_event here instead of using the one that the callback
        // is invoked with as that event is in window coordinates and callbacks
        // expect View coordinates.
        std::move(drop_cb).Run(drop_event, output_drag_op,
                               std::move(drag_image_layer_owner));
      },
      drop_event, std::move(drop_view_cb));
}

View* DropHelper::CalculateTargetView(const gfx::Point& root_view_location,
                                      const OSExchangeData& data,
                                      bool check_can_drop) {
  return CalculateTargetViewImpl(root_view_location, data, check_can_drop,
                                 nullptr);
}

View* DropHelper::CalculateTargetViewImpl(const gfx::Point& root_view_location,
                                          const OSExchangeData& data,
                                          bool check_can_drop,
                                          raw_ptr<View>* deepest_view) {
  View* view = root_view_->GetEventHandlerForPoint(root_view_location);
  if (view == deepest_view_) {
    // The view the mouse is over hasn't changed; reuse the target.
    return target_view_;
  }
  if (deepest_view)
    *deepest_view = view;
    // TODO(sky): for the time being these are separate. Once I port chrome menu
    // I can switch to the #else implementation and nuke the OS_WIN
    // implementation.
#if BUILDFLAG(IS_WIN)
  // View under mouse changed, which means a new view may want the drop.
  // Walk the tree, stopping at target_view_ as we know it'll accept the
  // drop.
  while (view && view != target_view_ &&
         (!view->GetEnabled() || !view->CanDrop(data))) {
    view = view->parent();
  }
#else
  int formats = 0;
  std::set<ui::ClipboardFormatType> format_types;
  while (view && view != target_view_) {
    if (view->GetEnabled() && view->GetDropFormats(&formats, &format_types) &&
        data.HasAnyFormat(formats, format_types) &&
        (!check_can_drop || view->CanDrop(data))) {
      // Found the view.
      return view;
    }
    formats = 0;
    format_types.clear();
    view = view->parent();
  }
#endif
  return view;
}

void DropHelper::NotifyDragEntered(const OSExchangeData& data,
                                   const gfx::Point& root_view_location,
                                   int drag_operation) {
  if (!target_view_)
    return;

  gfx::Point target_view_location(root_view_location);
  View::ConvertPointToTarget(root_view_, target_view_, &target_view_location);
  ui::DropTargetEvent enter_event(data, gfx::PointF(target_view_location),
                                  gfx::PointF(root_view_location),
                                  drag_operation);
  target_view_->OnDragEntered(enter_event);
}

int DropHelper::NotifyDragOver(const OSExchangeData& data,
                               const gfx::Point& root_view_location,
                               int drag_operation) {
  if (!target_view_)
    return ui::DragDropTypes::DRAG_NONE;

  gfx::Point target_view_location(root_view_location);
  View::ConvertPointToTarget(root_view_, target_view_, &target_view_location);
  ui::DropTargetEvent enter_event(data, gfx::PointF(target_view_location),
                                  gfx::PointF(root_view_location),
                                  drag_operation);
  return target_view_->OnDragUpdated(enter_event);
}

void DropHelper::NotifyDragExit() {
  if (target_view_)
    target_view_->OnDragExited();
}

}  // namespace views
