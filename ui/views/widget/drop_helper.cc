// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/drop_helper.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

const View* g_drag_entered_callback_view = nullptr;

base::RepeatingClosure* GetDragEnteredCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return callback.get();
}

}  // namespace

DropHelper::DropHelper(View* root_view)
    : root_view_(root_view), target_view_(nullptr), deepest_view_(nullptr) {}

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
  View* view = CalculateTargetViewImpl(root_view_location, data, true,
                                       &deepest_view_);

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

int DropHelper::OnDrop(const OSExchangeData& data,
                       const gfx::Point& root_view_location,
                       int drag_operation) {
  View* drop_view = target_view_;
  deepest_view_ = target_view_ = nullptr;
  if (!drop_view)
    return ui::DragDropTypes::DRAG_NONE;

  if (drag_operation == ui::DragDropTypes::DRAG_NONE) {
    drop_view->OnDragExited();
    return ui::DragDropTypes::DRAG_NONE;
  }

  gfx::Point view_location(root_view_location);
  View* root_view = drop_view->GetWidget()->GetRootView();
  View::ConvertPointToTarget(root_view, drop_view, &view_location);
  ui::DropTargetEvent drop_event(data, gfx::PointF(view_location),
                                 gfx::PointF(view_location), drag_operation);
  return drop_view->OnPerformDrop(drop_event);
}

View* DropHelper::CalculateTargetView(
    const gfx::Point& root_view_location,
    const OSExchangeData& data,
    bool check_can_drop) {
  return CalculateTargetViewImpl(root_view_location, data, check_can_drop,
                                 nullptr);
}

View* DropHelper::CalculateTargetViewImpl(
    const gfx::Point& root_view_location,
    const OSExchangeData& data,
    bool check_can_drop,
    View** deepest_view) {
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
#if defined(OS_WIN)
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
                                  gfx::PointF(target_view_location),
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
                                  gfx::PointF(target_view_location),
                                  drag_operation);
  return target_view_->OnDragUpdated(enter_event);
}

void DropHelper::NotifyDragExit() {
  if (target_view_)
    target_view_->OnDragExited();
}

}  // namespace views
